#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_event.h"
#include "esp_system.h"

#include "app_cmd.h"
#include "event_loops.h"
#include "storage.h"
#include "deviceinfo.h"

static const char *TAG = "cmd";

#define PROMPT_STR "SenseCAP"

int max(int a, int b) {
    return (a > b) ? a : b;
}

/** wifi set command **/
static struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_cfg_args;

static int wifi_cfg_set(int argc, char **argv)
{

    struct view_data_wifi_config cfg;

    memset(&cfg, 0, sizeof(cfg));

    int nerrors = arg_parse(argc, argv, (void **) &wifi_cfg_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_cfg_args.end, argv[0]);
        return 1;
    }

    if (wifi_cfg_args.ssid->count) {
        int len = strlen( wifi_cfg_args.ssid->sval[0] );
        if( len > (sizeof(cfg.ssid) - 1) ) { 
            ESP_LOGE(TAG,  "out of 31 bytes :%s", wifi_cfg_args.ssid->sval[0]);
            return -1;
        }
        strncpy( cfg.ssid, wifi_cfg_args.ssid->sval[0], max(len, 31) );
    } else {
        ESP_LOGE(TAG,  "no ssid");
        return -1;
    }

    if (wifi_cfg_args.password->count) {
        int len = strlen(wifi_cfg_args.password->sval[0]);
        if( len > (sizeof(cfg.password) - 1) ){ 
            ESP_LOGE(TAG,  "out of 64 bytes :%s", wifi_cfg_args.password->sval[0]);
            return -1;
        }
        cfg.have_password = true;
        strncpy( cfg.password, wifi_cfg_args.password->sval[0], len );
    }
    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_WIFI_CONNECT, &cfg, sizeof(struct view_data_wifi_config), portMAX_DELAY);
    return 0;
}
//wifi_cfg -s ssid -p password
static void register_cmd_wifi_sta(void)
{
    wifi_cfg_args.ssid =  arg_str0("s", NULL, "<ssid>", "SSID of AP");
    wifi_cfg_args.password =  arg_str0("p", NULL, "<password>", "password of AP");
    wifi_cfg_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "wifi_sta",
        .help = "WiFi is station mode, join specified soft-AP",
        .hint = NULL,
        .func = &wifi_cfg_set,
        .argtable = &wifi_cfg_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/************* device info get and set **************/
static struct {
    struct arg_str *eui;
    struct arg_str *key;
    struct arg_end *end;
} deviceinfo_cfg_args;

static int deviceinfo_cmd(int argc, char **argv)
{
    struct view_data_deviceinfo cfg;
    bool change = false;
    esp_err_t ret;
    memset(&cfg, 0, sizeof(cfg));

    int nerrors = arg_parse(argc, argv, (void **) &deviceinfo_cfg_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, deviceinfo_cfg_args.end, argv[0]);
        return 1;
    }

    if (deviceinfo_cfg_args.eui->count) {
        int len = strlen( deviceinfo_cfg_args.eui->sval[0] );
        if( len != 16 ) { 
            ESP_LOGE(TAG,  "must be 16 bytes :%s", deviceinfo_cfg_args.eui->sval[0]);
            return -1;
        }
        change = true;
        strcpy( cfg.eui, deviceinfo_cfg_args.eui->sval[0] );
    }

    if (deviceinfo_cfg_args.key->count) {
        int len = strlen(deviceinfo_cfg_args.key->sval[0]);
        if( len != 32 ){ 
            ESP_LOGE(TAG,  "must be 32 bytes :%s", deviceinfo_cfg_args.key->sval[0]);
            return -1;
        }
        change = true;
        strncpy( cfg.key, deviceinfo_cfg_args.key->sval[0], len );
    }

    if( change ) {
        deviceinfo_set(&cfg);
    }

    ret = deviceinfo_get(&cfg);
    if( ret == ESP_OK ) {
        ESP_LOGI(TAG, "eui: %s", cfg.eui);
        ESP_LOGI(TAG, "key: %s", cfg.key);
    } else {
        ESP_LOGE(TAG, "deviceinfo read fail %d!", ret);
    }
    return 0;
}

static void register_cmd_deviceinfo(void)
{
    deviceinfo_cfg_args.eui =  arg_str0("e", NULL, "<eui>", "EUI");
    deviceinfo_cfg_args.key =  arg_str0("k", NULL, "<key>", "KEY");
    deviceinfo_cfg_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "deviceinfo",
        .help = "deviceinfo get/set",
        .hint = NULL,
        .func = &deviceinfo_cmd,
        .argtable = &deviceinfo_cfg_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/************* reboot **************/
static int do_reboot(int argc, char **argv)
{
    esp_restart();
}

static void register_cmd_reboot(void)
{
    const esp_console_cmd_t cmd = {
        .command = "reboot",
        .help = "reboot the device",
        .hint = NULL,
        .func = &do_reboot,
        .argtable = NULL
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

int app_cmd_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    register_cmd_wifi_sta();
    register_cmd_deviceinfo();
    register_cmd_reboot();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return 0;
}