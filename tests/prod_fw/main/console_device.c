/*
 * SPDX-FileCopyrightText: 2024 Seeed Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip6.h"
#include "lwip/opt.h"
#include "esp_wifi.h"

#include "esp_bt_device.h"

#include "sensecap-watcher.h"

#include "console_device.h"

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR     "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_device", .plugin_regd_fn = &console_cmd_device_register };

typedef struct device_op_t
{
    char *name;
    esp_err_t (*operation)(struct device_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} device_op_t;

static esp_err_t device_help_op(device_op_t *self, int argc, char *argv[]);
static esp_err_t device_get_ip_op(device_op_t *self, int argc, char *argv[]);
static esp_err_t device_get_mac_op(device_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_device";

static device_op_t cmd_list[] = {
    { .name = "help", .operation = device_help_op, .arg_cnt = 2, .start_index = 1, .help = "device help: Prints the help text for all device commands" },
    { .name = "ip", .operation = device_get_ip_op, .arg_cnt = 2, .start_index = 1, .help = "device get ip: Gets the IP address of the device" },
    { .name = "mac", .operation = device_get_mac_op, .arg_cnt = 2, .start_index = 1, .help = "device get mac: Gets the MAC address of the device" },
};

static esp_err_t device_help_op(device_op_t *self, int argc, char *argv[])
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);

    for (int i = 0; i < cmd_count; i++)
    {
        if ((cmd_list[i].help != NULL) && (strlen(cmd_list[i].help) != 0))
        {
            printf(" %s\n", cmd_list[i].help);
        }
    }

    return ESP_OK;
}

static esp_err_t device_get_ip_op(device_op_t *self, int argc, char *argv[])
{
    // TODO
    printf("ip: 0.0.0.0\n");
    printf("Seeed cmd test over\r\n");
    return ESP_OK;
}

static esp_err_t device_get_mac_op(device_op_t *self, int argc, char *argv[])
{
    uint8_t mac[6] = { 0 };

    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
    {
        printf("sta mac: " MACSTR "\n", MAC2STR(mac));
    }

    if (esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK)
    {
        printf("ap  mac: " MACSTR "\n", MAC2STR(mac));
    }

    const uint8_t *bt_mac = esp_bt_dev_get_address();
    if (bt_mac != NULL)
    {
        printf("bt  mac: " MACSTR "\n", MAC2STR(bt_mac));
    }
    printf("Seeed cmd test over\r\n");
    return ESP_OK;
}

/* handle 'device' command */
static esp_err_t do_cmd_device(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    device_op_t cmd;

    for (int i = 0; i < cmd_count; i++)
    {
        cmd = cmd_list[i];

        if (argc < cmd.start_index + 1)
        {
            continue;
        }

        if (!strcmp(cmd.name, argv[cmd.start_index]))
        {
            /* Get interface for eligible commands */
            if (cmd.arg_cnt == argc)
            {
                if (cmd.operation != NULL)
                {
                    if (cmd.operation(&cmd, argc, argv) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Usage:\n%s", cmd.help);
                        return 0;
                    }
                }
                return ESP_OK;
            }
        }
    }

    ESP_LOGE(TAG, "Command not available");

    return ESP_OK;
}

/**
 * @brief Registers the device command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_device_register(void)
{
    esp_err_t ret;

    esp_console_cmd_t command = { .command = "device", .help = "Command for device operations\n For more info run 'device help'", .func = &do_cmd_device };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register device");
    }

    return ret;
}
