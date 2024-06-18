#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_wifi.h"
#include "esp_app_desc.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "sensecap-watcher.h"

#include "app_device_info.h"
#include "event_loops.h"
#include "data_defs.h"
#include "storage.h"
#include "app_rgb.h"
#include "app_audio.h"
#include "audio_player.h"
#include "app_sensecraft.h"
#include "factory_info.h"
#include "tf_module_ai_camera.h"
#include "util.h"

#define APP_DEVICE_INFO_MAX_STACK 4096
#define BRIGHTNESS_STORAGE_KEY    "brightness"
#define SOUND_STORAGE_KEY         "sound"
#define RGB_SWITCH_STORAGE_KEY    "rgbswitch"
#define CLOUD_SERVICE_STORAGE_KEY "cssk"
#define AI_SERVICE_STORAGE_KEY    "aiservice"
#define USAGE_GUIDE_SK            "usage_guide"
#define TIME_AUTOMATIC_SK         "time_auto"
static const char *TAG = "deviceinfo";

static uint8_t SN[9] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
static uint8_t EUI[8] = { 0 };
static uint8_t DEVCODE[8] = { 0 };
static uint8_t QRCODE[67] = { 0 };

int server_code = 1;
int create_batch = 1000205;

int brightness = 100;
int brightness_past = 100;

int sound_value = 80;
int sound_value_past = 80;

int rgb_switch = 1;
int rgb_switch_past = 1;

int cloud_service_switch = 1;
int cloud_service_switch_past = 1;

int usage_guide_switch = 0;
int usage_guide_switch_past = 0;


static sscma_client_info_t *g_himax_info;

// ai service ip for mqtt
ai_service_pack ai_service;
ai_service_pack ai_service_past;

SemaphoreHandle_t MUTEX_brightness;
SemaphoreHandle_t MUTEX_rgb_switch;
SemaphoreHandle_t MUTEX_sound;
SemaphoreHandle_t MUTEX_ai_service;
SemaphoreHandle_t MUTEX_cloud_service_switch;
SemaphoreHandle_t MUTEX_usage_guide;
SemaphoreHandle_t MUTEX_sdcard_flash_status;

static StackType_t *app_device_info_task_stack = NULL;
static StaticTask_t app_device_info_task_buffer;

static struct view_data_device_status g_device_status;
static struct view_data_sdcard_flash_status g_sdcard_flash_status;
static volatile atomic_bool g_mqttconn = ATOMIC_VAR_INIT(false);
static volatile atomic_bool g_timeout_firstreport = ATOMIC_VAR_INIT(false);
static volatile atomic_bool g_will_reset_factory = ATOMIC_VAR_INIT(false);

static esp_timer_handle_t g_timer_firstreport;


void app_device_info_task(void *pvParameter);

/*----------------------------------------------------tool function---------------------------------------------*/
void byteArrayToHexString(const uint8_t *byteArray, size_t byteArraySize, char *hexString)
{
    for (size_t i = 0; i < byteArraySize; ++i)
    {
        sprintf(&hexString[2 * i], "%02X", byteArray[i]);
    }
}

void string_to_byte_array(const char *str, uint8_t *byte_array, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        sscanf(str + 2 * i, "%2hhx", &byte_array[i]);
    }
}

/*----------------------------------------------------------init function--------------------------------------*/

void init_sn_from_nvs()
{
    const char *sn_str = factory_info_sn_get();
    string_to_byte_array(sn_str, SN, 9);
}

void init_eui_from_nvs()
{
    const char *eui_str = factory_info_eui_get();
    const char *code_str = factory_info_code_get();
    if (eui_str == NULL || code_str == NULL)
    {
        ESP_LOGE(TAG, "Failed to get factory information of EUI and code \n");
        return;
    }

    uint8_t code[8];
    string_to_byte_array(eui_str, EUI, 8);
    string_to_byte_array(code_str, DEVCODE, 8);
}

void init_batchid_from_nvs()
{
    const char *batchid = factory_info_batchid_get();
    create_batch = atoi(batchid);
    return;
}

void init_server_code_from_nvs()
{
    uint8_t platform = factory_info_platform_get();
    server_code = (int)platform;
    return;
}

void init_ai_service_param_from_nvs()
{
    size_t size = sizeof(ai_service);
    esp_err_t ret = storage_read(AI_SERVICE_STORAGE_KEY, &ai_service, &size);
    if (ret == ESP_OK)
    {
        ai_service_past = ai_service;
        ESP_LOGI(TAG, "ai_service value loaded from NVS: %d", ai_service);
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "No ai_service value found in NVS. Using default: %d", ai_service);
    }
    else
    {
        ESP_LOGE(TAG, "Error reading ai_service from NVS: %s", esp_err_to_name(ret));
    }
}

void init_rgb_switch_from_nvs()
{
    size_t size = sizeof(rgb_switch);
    esp_err_t ret = storage_read(RGB_SWITCH_STORAGE_KEY, &rgb_switch, &size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "rgb_switch value loaded from NVS: %d", rgb_switch);
        rgb_switch_past = rgb_switch;
        if (rgb_switch == 1)
        {
            set_rgb_with_priority(UI_CALLER, on);
        }
        else
        {
            set_rgb_with_priority(UI_CALLER, off);
        }
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "No rgb_switch value found in NVS. Using default: %d", rgb_switch);
        if (rgb_switch == 1)
        {
            set_rgb_with_priority(UI_CALLER, on);
        }
        else
        {
            set_rgb_with_priority(UI_CALLER, off);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error reading rgb_switch from NVS: %s", esp_err_to_name(ret));
    }
}

void init_brightness_from_nvs()
{
    size_t size = sizeof(brightness);
    esp_err_t ret = storage_read(BRIGHTNESS_STORAGE_KEY, &brightness, &size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Brightness value loaded from NVS: %d", brightness);
        ret = bsp_lcd_brightness_set(brightness);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "LCD brightness set err:%d", ret);
        }
        brightness_past = brightness;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No brightness value found in NVS. Using default: %d", brightness);
        ret = bsp_lcd_brightness_set(brightness);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "LCD brightness set err:%d", ret);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error reading brightness from NVS: %s", esp_err_to_name(ret));
    }
}

void init_sound_from_nvs()
{
    size_t size = sizeof(sound_value);
    esp_err_t ret = storage_read(SOUND_STORAGE_KEY, &sound_value, &size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Sound value loaded from NVS: %d", sound_value);
        ret = bsp_codec_volume_set(sound_value, NULL);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "sound value set err:%d", ret);
        }
        sound_value_past = sound_value;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "No sound value found in NVS. Using default: %d", sound_value);
    }
    else
    {
        ESP_LOGE(TAG, "Error reading sound value from NVS: %s", esp_err_to_name(ret));
    }
}

void init_cloud_service_switch_from_nvs()
{
    size_t size = sizeof(cloud_service_switch);
    esp_err_t ret = storage_read(CLOUD_SERVICE_STORAGE_KEY, &cloud_service_switch, &size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "cloud_service_switch value loaded from NVS: %d", cloud_service_switch);
        cloud_service_switch_past = cloud_service_switch;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No cloud_service_switch value found in NVS. Using default: %d", cloud_service_switch);
    }
    else
    {
        ESP_LOGE("NVS", "Error reading rgb_switch from NVS: %s", esp_err_to_name(ret));
    }
}

void init_usage_guide_switch_from_nvs()
{
    size_t size = sizeof(usage_guide_switch);
    esp_err_t ret = storage_read(USAGE_GUIDE_SK, &usage_guide_switch, &size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "usage_guide_switch value loaded from NVS: %d", usage_guide_switch);
        usage_guide_switch_past = usage_guide_switch;
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No usage_guide_switch value found in NVS. Using default: %d", usage_guide_switch);
    }
    else
    {
        ESP_LOGE("NVS", "Error reading usage_guide_switch from NVS: %s", esp_err_to_name(ret));
    }
}

void init_qrcode_content()
{
    char str_platformid[4] = { 0 };
    char str_batchid[20] = { 0 };
    snprintf(str_platformid, sizeof(str_platformid), "%d", server_code);
    snprintf(str_batchid, sizeof(str_batchid), "%d", create_batch);
    char hexStringEUI[19] = { 0 };
    char hexStringCode[19] = { 0 };
    char hexStringSn[19] = { 0 };
    byteArrayToHexString(EUI, sizeof(EUI), hexStringEUI);
    byteArrayToHexString(DEVCODE, sizeof(DEVCODE), hexStringCode);
    byteArrayToHexString(SN, sizeof(SN), hexStringSn);

    snprintf((char *)QRCODE, sizeof(QRCODE), "w1:%s%s:%s:%s:%s", hexStringEUI, hexStringCode, str_platformid, str_batchid, hexStringSn);
    //esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_SN_CODE, QRCODE, sizeof(QRCODE), pdMS_TO_TICKS(10000));
}

/*----------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------GET FACTORY cfg----------------------------------------------------------*/

uint8_t *get_sn(int caller)
{
    return SN;
}

uint8_t *get_eui()
{
    return EUI;
}

uint8_t *get_qrcode_content()
{
    return QRCODE;
}

uint8_t *get_bt_mac()
{
    const uint8_t *bd_addr = esp_bt_dev_get_address();
    if (bd_addr)
    {
        ESP_LOGI(TAG, "Bluetooth MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get Bluetooth MAC Address");
    }
    return bd_addr;
}

uint8_t *get_wifi_mac()
{
    static uint8_t wifi_mac[6] = { 0 };

    if (wifi_mac[0] == 0 && wifi_mac[1] == 0 && wifi_mac[2] == 0) {
        esp_wifi_get_mac(WIFI_IF_STA, wifi_mac);
        ESP_LOGI(TAG, "WiFi MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3], wifi_mac[4], wifi_mac[5]);
    }
    return wifi_mac;
}

/*---------------------------------------------version module--------------------------------------------------------------*/

char *get_software_version(int caller)
{
    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "BLE get software version");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get software version");
            char final_string[150];
            memset(final_string, 0, sizeof(final_string));
            snprintf(final_string, sizeof(final_string), "v%s", g_device_status.fw_version);
            ESP_LOGD(TAG, "Software Version: %s\n", final_string);
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_SOFTWARE_VERSION_CODE, 
                              final_string, strlen(final_string) + 1, portMAX_DELAY);
            break;
    }

    return g_device_status.fw_version;
}

char *get_himax_software_version(int caller)
{
    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "BLE get himax software version");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get himax software version");
            char final_string[150];
            memset(final_string, 0, sizeof(final_string));
            snprintf(final_string, sizeof(final_string), "v%s", g_device_status.himax_fw_version);
            ESP_LOGD(TAG, "Himax Software Version: %s\n", final_string);
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_HIMAX_SOFTWARE_VERSION_CODE, 
                              final_string, strlen(final_string) + 1, portMAX_DELAY);
            break;
    }

    return g_device_status.himax_fw_version;
}

/*----------------------------------------------brightness module------------------------------------------------------*/
int get_brightness(int caller)
{
    int brightness2 = brightness;  //copy here

    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "BLE get brightness");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get brightness");
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_BRIGHTNESS, &brightness2, sizeof(int), pdMS_TO_TICKS(10000));
            break;
    }

    return brightness2;
}

esp_err_t set_brightness(int caller, int value)
{
    ESP_LOGI(TAG, "set_brightness: %d", value);
    xSemaphoreTake(MUTEX_brightness, portMAX_DELAY);
    brightness_past = brightness;
    brightness = value;
    xSemaphoreGive(MUTEX_brightness);
    return ESP_OK;
}

static esp_err_t __set_brightness()
{
    xSemaphoreTake(MUTEX_brightness, portMAX_DELAY);
    if (brightness_past != brightness)
    {
        ESP_RETURN_ON_ERROR(storage_write(BRIGHTNESS_STORAGE_KEY, &brightness, sizeof(brightness)), TAG, "set_brightness cfg write err");
        ESP_RETURN_ON_ERROR(bsp_lcd_brightness_set(brightness), TAG, "bsp_lcd_brightness_set err");
        brightness_past = brightness;
        ESP_LOGD(TAG, "set_brightness done: %d", brightness);
    }
    xSemaphoreGive(MUTEX_brightness);

    return ESP_OK;
}

/*--------------------------------------------rgb switch  module----------------------------------------------------------------*/

int get_rgb_switch(int caller)
{
    int rgb_switch2 = rgb_switch;

    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "BLE get rgb_switch");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get rgb_switch");
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_RGB_SWITCH, &rgb_switch2, sizeof(int), pdMS_TO_TICKS(10000));
            break;
    }

    return rgb_switch2;
}

esp_err_t set_rgb_switch(int caller, int value)
{
    ESP_LOGI(TAG, "set_rgb_switch: %d", value);
    xSemaphoreTake(MUTEX_rgb_switch, portMAX_DELAY);
    rgb_switch_past = rgb_switch;
    rgb_switch = value;
    xSemaphoreGive(MUTEX_rgb_switch);
    return ESP_OK;
}

static esp_err_t __set_rgb_switch()
{
    xSemaphoreTake(MUTEX_rgb_switch, portMAX_DELAY);
    if (rgb_switch_past != rgb_switch)
    {
        ESP_RETURN_ON_ERROR(storage_write(BRIGHTNESS_STORAGE_KEY, &brightness, sizeof(brightness)), TAG, "set_rgb_switch cfg write err");
        set_rgb_with_priority(UI_CALLER, rgb_switch == 1 ? on : off);
        rgb_switch_past = rgb_switch;
        ESP_LOGD(TAG, "set_rgb_switch done: %d", rgb_switch);
    }
    xSemaphoreGive(MUTEX_rgb_switch);

    return ESP_OK;
}

/*-----------------------------------------------------sound_Volume---------------------------------------------------*/

int get_sound(int caller)
{
    int sound2 = sound_value;

    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "AT_CMD_CALLER get sound");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get sound");
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_SOUND, &sound2, sizeof(int), pdMS_TO_TICKS(10000));
            break;
    }

    return sound2;
}

esp_err_t set_sound(int caller, int value)
{
    ESP_LOGI(TAG, "set_sound: %d", value);
    xSemaphoreTake(MUTEX_sound, portMAX_DELAY);
    sound_value_past = sound_value;
    sound_value = value;
    xSemaphoreGive(MUTEX_sound);
    return ESP_OK;
}

static esp_err_t __set_sound()
{
    xSemaphoreTake(MUTEX_sound, portMAX_DELAY);
    if (sound_value_past != sound_value)
    {
        ESP_RETURN_ON_ERROR(storage_write(SOUND_STORAGE_KEY, &sound_value, sizeof(sound_value)), TAG, "set_sound cfg write err");
        ESP_RETURN_ON_ERROR(bsp_codec_volume_set(sound_value, NULL), TAG, "bsp_codec_volume_set err");
        sound_value_past = sound_value;
        ESP_LOGD(TAG, "set_sound done: %d", sound_value);
    }
    xSemaphoreGive(MUTEX_sound);

    return ESP_OK;
}

/*-----------------------------------------------------Cloud_service_switch------------------------------------------*/
int get_cloud_service_switch(int caller)
{
    int cloud_service_switch2 = cloud_service_switch;

    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "AT_CMD_CALLER get cloud_service_switch");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get cloud_service_switch");
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_CLOUD_SERVICE_SWITCH, 
                              &cloud_service_switch2, sizeof(int), portMAX_DELAY);   
            break;
    }

    return cloud_service_switch2;
}

esp_err_t set_cloud_service_switch(int caller, int value)
{
    ESP_LOGI(TAG, "set_cloud_service_switch: %d", value);
    xSemaphoreTake(MUTEX_cloud_service_switch, portMAX_DELAY);
    cloud_service_switch_past = cloud_service_switch;
    cloud_service_switch = value;
    xSemaphoreGive(MUTEX_cloud_service_switch);
    return ESP_OK;
}

static esp_err_t __set_cloud_service_switch()
{
    xSemaphoreTake(MUTEX_cloud_service_switch, portMAX_DELAY);
    if (cloud_service_switch_past != cloud_service_switch)
    {
        ESP_RETURN_ON_ERROR(storage_write(CLOUD_SERVICE_STORAGE_KEY, &cloud_service_switch, sizeof(cloud_service_switch)), TAG, 
                            "set_sound cfg write err");
        cloud_service_switch_past = cloud_service_switch;
        ESP_LOGD(TAG, "set_cloud_service_switch done: %d", cloud_service_switch);
    }
    xSemaphoreGive(MUTEX_cloud_service_switch);

    return ESP_OK;
}
/*----------------------------------------------------AI_service_package----------------------------------------------*/
#if 0
// TODO: need rethink

ai_service_pack *get_ai_service(int caller)
{
    if (xSemaphoreTake(MUTEX_ai_service, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "get_ai_service: MUTEX_ai_service take failed");
        return NULL;
    }
    ai_service_pack *result = NULL;
    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "BLE get ai_service");
            result = &ai_service;
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI get ai_service");
            result = &ai_service;
            break;
    }
    xSemaphoreGive(MUTEX_ai_service);
    return result;
}

esp_err_t set_ai_service(int caller, ai_service_pack value)
{
    if (xSemaphoreTake(MUTEX_ai_service, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "set_ai_service: MUTEX_ai_service take failed");
        return;
    }
    ai_service_past = ai_service;
    ai_service = value;

    xSemaphoreGive(MUTEX_ai_service);
}

static int __set_ai_service()
{
    if (xSemaphoreTake(MUTEX_ai_service, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "set_ai_service: MUTEX_ai_service take failed");
        return -1;
    }
    if (memcmp(&ai_service_past, &ai_service, sizeof(ai_service_pack)) != 0)
    {
        esp_err_t ret = storage_write(AI_SERVICE_STORAGE_KEY, &ai_service, sizeof(ai_service));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "set_ai_service cfg write err:%d", ret);
            xSemaphoreGive(MUTEX_ai_service);
            return ret;
        }
    }
    xSemaphoreGive(MUTEX_ai_service);
    return 0;
}
#endif

/*----------------------------------------------------usage guide switch--------------------------------------------------------*/

int get_usage_guide(int caller)
{
    int usage_guide_switch2 = usage_guide_switch;

    switch (caller)
    {
        case AT_CMD_CALLER:
            ESP_LOGI(TAG, "AT_CMD_CALLER  get_usage_guide");
            break;
        case UI_CALLER:
            ESP_LOGI(TAG, "UI  get_usage_guide");
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_USAGE_GUIDE_SWITCH, 
                              &usage_guide_switch2, sizeof(int), portMAX_DELAY);
            break;
    }

    return usage_guide_switch2;
}

esp_err_t set_usage_guide(int caller, int value)
{
    xSemaphoreTake(MUTEX_usage_guide, portMAX_DELAY);
    usage_guide_switch_past = usage_guide_switch;
    usage_guide_switch = value;
    xSemaphoreGive(MUTEX_usage_guide);
    return ESP_OK;
}

static esp_err_t __set_usage_guide()
{
    xSemaphoreTake(MUTEX_usage_guide, portMAX_DELAY);
    if (usage_guide_switch_past != usage_guide_switch)
    {
        ESP_RETURN_ON_ERROR(storage_write(USAGE_GUIDE_SK, &usage_guide_switch, sizeof(usage_guide_switch)), 
                            TAG, "__set_usage_guide cfg write err");
        usage_guide_switch_past = usage_guide_switch;
        ESP_LOGD(TAG, "set_usage_guide done: %d", usage_guide_switch);
    }
    xSemaphoreGive(MUTEX_usage_guide);
    return ESP_OK;
}


/*----------------------------------------------------reset-factory--------------------------------------------------------*/

esp_err_t set_reset_factory()
{
    atomic_store(&g_will_reset_factory, true);
    return ESP_OK;
}

static esp_err_t __check_reset_factory()
{
    if (atomic_load(&g_will_reset_factory))
    {
        ESP_LOGI(TAG, "###########>>> start to reset factory <<<###########");
        storage_erase();
        esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_REBOOT, NULL, 0, pdMS_TO_TICKS(10000));
        atomic_store(&g_will_reset_factory, false);
    }
    return ESP_OK;
}


/*-----------------------------------------------------time_auto_update-----------------------------------------------*/
/**
 * will be deprecated!
*/
int get_time_automatic(int caller)
{
    return 1;
}

esp_err_t set_time_automatic(int caller, int value)
{
    return ESP_OK;
}

/*------------------------------------------------------sdcard into------------------------------------------------------*/
uint16_t get_spiffs_total_size(int caller)
{
    uint16_t size = 0;
    xSemaphoreTake(MUTEX_sdcard_flash_status, portMAX_DELAY);
    size = g_sdcard_flash_status.spiffs_total_KiB;
    xSemaphoreGive(MUTEX_sdcard_flash_status);

    return size;
}

uint16_t get_spiffs_free_size(int caller)
{
    uint16_t size = 0;
    xSemaphoreTake(MUTEX_sdcard_flash_status, portMAX_DELAY);
    size = g_sdcard_flash_status.spiffs_free_KiB;
    xSemaphoreGive(MUTEX_sdcard_flash_status);

    return size;
}

uint16_t get_sdcard_total_size(int caller)
{
    uint16_t size = 0;
    xSemaphoreTake(MUTEX_sdcard_flash_status, portMAX_DELAY);
    size = g_sdcard_flash_status.sdcard_total_MiB;
    xSemaphoreGive(MUTEX_sdcard_flash_status);

    return size;
}

uint16_t get_sdcard_free_size(int caller)
{
    uint16_t size = 0;
    xSemaphoreTake(MUTEX_sdcard_flash_status, portMAX_DELAY);
    size = g_sdcard_flash_status.sdcard_free_MiB;
    xSemaphoreGive(MUTEX_sdcard_flash_status);

    return size;
}

/*-----------------------------------------------------TASK----------------------------------------------------------*/
void __try_check_sdcard_flash()
{
    size_t total = 0, used = 0;
    uint64_t sdtotal = 0, sdfree = 0;

    if (g_sdcard_flash_status.spiffs_total_KiB == 0)
    {
        // the partition label is hard coded
        esp_spiffs_info("storage", &total, &used);
    }
    if (g_sdcard_flash_status.sdcard_total_MiB == 0)
    {
        esp_vfs_fat_info(DRV_BASE_PATH_SD, &sdtotal, &sdfree);
    }

    xSemaphoreTake(MUTEX_sdcard_flash_status, portMAX_DELAY);
    if (g_sdcard_flash_status.spiffs_total_KiB == 0 && total > 0)
    {
        g_sdcard_flash_status.spiffs_total_KiB = (uint16_t)(total / 1024);
        g_sdcard_flash_status.spiffs_free_KiB = (uint16_t)((total - used) / 1024);
        ESP_LOGI(TAG, "spiffs total %d KiB, free %d KiB", (int)g_sdcard_flash_status.spiffs_total_KiB, (int)g_sdcard_flash_status.spiffs_free_KiB);
    }
    if (g_sdcard_flash_status.sdcard_total_MiB == 0 && sdtotal > 0)
    {
        g_sdcard_flash_status.sdcard_total_MiB = (uint16_t)(sdtotal / 1024 / 1024);
        g_sdcard_flash_status.sdcard_free_MiB = (uint16_t)(sdfree / 1024 / 1024);
        ESP_LOGI(TAG, "sdcard total %d MiB, free %d MiB", (int)g_sdcard_flash_status.sdcard_total_MiB, (int)g_sdcard_flash_status.sdcard_free_MiB);
    }
    xSemaphoreGive(MUTEX_sdcard_flash_status);
}

static void __timer_cb_first_report(void *arg)
{
    atomic_store(&g_timeout_firstreport, true);
}

void __app_device_info_task(void *pvParameter)
{
    uint8_t batnow = 0;
    uint32_t cnt = 0;
    bool firstboot_reported = false, himax_version_got = false;
    static uint8_t last_charge_st = 0x66, last_sdcard_inserted = 0x88, sdcard_debounce = 0x99;
    static uint8_t last_bat_level_report = 255;


    MUTEX_brightness = xSemaphoreCreateMutex();
    MUTEX_rgb_switch = xSemaphoreCreateMutex();
    MUTEX_sound = xSemaphoreCreateMutex();
    MUTEX_cloud_service_switch = xSemaphoreCreateMutex();
    MUTEX_ai_service = xSemaphoreCreateMutex();
    MUTEX_usage_guide = xSemaphoreCreateMutex();
    MUTEX_sdcard_flash_status = xSemaphoreCreateMutex();

    init_brightness_from_nvs();
    init_rgb_switch_from_nvs();
    init_sound_from_nvs();
    init_cloud_service_switch_from_nvs();
    init_ai_service_param_from_nvs();

    // get spiffs and sdcard status
    __try_check_sdcard_flash();

    g_device_status.battery_per = bsp_battery_get_percent();

    //post battery level to UI at early time
    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_BATTERY_ST, 
                        &g_device_status, sizeof(struct view_data_device_status), portMAX_DELAY);

    while (1)
    {
        __set_cloud_service_switch();
        __set_brightness();
        __set_rgb_switch();
        __set_sound();
        __set_usage_guide();
        __check_reset_factory();
        //__set_ai_service();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        cnt++;

        if (!himax_version_got && !atomic_load(&g_timeout_firstreport)) {
            char *himax_version = tf_module_ai_camera_himax_version_get();

            if (himax_version && strlen(himax_version) > 0) {
                g_device_status.himax_fw_version = himax_version;
                ESP_LOGI(TAG, "Got Himax fw version: %s", g_device_status.himax_fw_version);
                himax_version_got = true;
            } else {
                ESP_LOGW(TAG, "Failed to get himax info [%d] ...", cnt);
            }
        }

        if (!firstboot_reported && atomic_load(&g_timeout_firstreport))
        {
            last_bat_level_report = g_device_status.battery_per;
            app_sensecraft_mqtt_report_device_status(&g_device_status);
            firstboot_reported = true;
        }

        if ((cnt % 300) == 0)
        {
            batnow = bsp_battery_get_percent();
            if (abs(g_device_status.battery_per - batnow) > 0 || batnow == 0) {
                g_device_status.battery_per = batnow;
                esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_BATTERY_ST, 
                                    &g_device_status, sizeof(struct view_data_device_status), portMAX_DELAY);
            }
            // mqtt pub
            if (firstboot_reported && 
                (abs(last_bat_level_report - batnow) > 10 || batnow == 0 || (batnow == 100 && abs(last_bat_level_report - batnow) > 0))) {
                g_device_status.battery_per = batnow;
                app_sensecraft_mqtt_report_device_status(&g_device_status);
                last_bat_level_report = batnow;
            }
            if (batnow == 0) {
                vTaskDelay(pdMS_TO_TICKS(2000)); //for mqtt pub
                ESP_LOGW(TAG, "the battery drop to 0%%, will shutdown to protect the battery and data...");
                esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_BAT_DRAIN_SHUTDOWN, NULL, 0, pdMS_TO_TICKS(10000));
            }
            //TODO: open this after problem fignured
            //bsp_rtc_set_timer(62);  // feed the watchdog, leave 2sec overhead for iteration cost
        }

        if ((cnt % 5) == 0)
        {
            uint8_t chg = (uint8_t)(bsp_exp_io_get_level(BSP_PWR_VBUS_IN_DET) == 0);
            // ESP_LOGD(TAG, "charging: %d", chg);
            if (chg != last_charge_st)
            {
                last_charge_st = chg;
                esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_CHARGE_ST, &last_charge_st, 1, pdMS_TO_TICKS(10000));
                if (!chg) {  //measure the battery immediately when unplug the usb-c charger
                    batnow = bsp_battery_get_percent();
                    if (abs(g_device_status.battery_per - batnow) > 1 || batnow == 0) {
                        g_device_status.battery_per = batnow;
                        esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_BATTERY_ST, 
                                            &g_device_status, sizeof(struct view_data_device_status), portMAX_DELAY);
                    }
                }
            }
        }

        if ((cnt % 10) == 0)
        {
            uint8_t sdcard_inserted = (uint8_t)bsp_sdcard_is_inserted();
            if (sdcard_inserted == sdcard_debounce)
            {
                if (sdcard_inserted != last_sdcard_inserted)
                {
                    if (sdcard_inserted)
                    {
                        bsp_sdcard_init_default(); // sdcard might be initialized in board_init(), but it's ok
                        __try_check_sdcard_flash();
                    }
                    else
                    {
                        bsp_sdcard_deinit_default();
                        ESP_LOGW(TAG, "SD card is umounted.");
                        xSemaphoreTake(MUTEX_sdcard_flash_status, portMAX_DELAY);
                        g_sdcard_flash_status.sdcard_total_MiB = g_sdcard_flash_status.sdcard_free_MiB = 0;
                        xSemaphoreGive(MUTEX_sdcard_flash_status);
                    }
                    last_sdcard_inserted = sdcard_inserted;
                }
            }
            sdcard_debounce = sdcard_inserted;
        }
    }
}

static void __event_loop_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch (id)
    {
        case CTRL_EVENT_MQTT_CONNECTED: {
            ESP_LOGI(TAG, "rcv event: CTRL_EVENT_MQTT_CONNECTED");
            atomic_store(&g_mqttconn, true);
            //should report device info no matter himax version is ready or not
            esp_timer_start_once(g_timer_firstreport, 2*1000000);
            break;
        }
        default:
            break;
    }
}

void app_device_info_init_early()
{
#if CONFIG_ENABLE_FACTORY_FW_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    // init critical info
    init_sn_from_nvs();
    init_eui_from_nvs();
    init_batchid_from_nvs();
    init_server_code_from_nvs();
    init_qrcode_content();
    init_usage_guide_switch_from_nvs();

    ESP_LOGI(TAG, "device info init early done, qrcode content: %s", (char *)get_qrcode_content());
}

void app_device_info_init()
{
    memset(&g_device_status, 0, sizeof(struct view_data_device_status));
    const esp_app_desc_t *app_desc = esp_app_get_description();
    // if newer hw_version come up in the future, we can tell it from the EUI
    // for this version firmware, we hard code hw_version as 1.0
    g_device_status.hw_version = "1.0";
    g_device_status.fw_version = app_desc->version;
    g_device_status.battery_per = 100;
    g_device_status.himax_fw_version = "0.0";

    memset(&g_sdcard_flash_status, 0, sizeof(struct view_data_sdcard_flash_status));

    const int stack_size = 10 * 1024;
    StackType_t *task_stack = (StackType_t *)psram_calloc(1, stack_size * sizeof(StackType_t));
    StaticTask_t *task_tcb = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    xTaskCreateStatic(__app_device_info_task, "app_device_info", stack_size, NULL, 5, task_stack, task_tcb);

    esp_event_handler_register_with(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_MQTT_CONNECTED, __event_loop_handler, NULL);

    // init a timer for sending deviceinfo finally, even if fail to get himax info
    esp_timer_create_args_t timer0args = {.callback = __timer_cb_first_report};
    esp_timer_create(&timer0args, &g_timer_firstreport);
}
