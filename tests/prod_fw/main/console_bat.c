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

#include "sensecap-watcher.h"

#include "console_bat.h"


/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_bat", .plugin_regd_fn = &console_cmd_bat_register };

typedef struct bat_op_t
{
    char *name;
    esp_err_t (*operation)(struct bat_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} bat_op_t;


static esp_err_t bat_help_op(bat_op_t *self, int argc, char *argv[]);
static esp_err_t bat_get_op(bat_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_bat";

static bat_op_t cmd_list[] = {
    { .name = "help", .operation = bat_help_op, .arg_cnt = 2, .start_index = 1, .help = "bat help: Prints the help text for all device commands" },
    { .name = "get", .operation = bat_get_op, .arg_cnt = 2, .start_index = 1, .help = "bat get: Gets the bat volt" },
};

static esp_err_t bat_help_op(bat_op_t *self, int argc, char *argv[])
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);

    for (int i = 0; i < cmd_count; i++)
    {
        if ((cmd_list[i].help != NULL) && (strlen(cmd_list[i].help) != 0))
        {
            printf(" %s\n", cmd_list[i].help);
        }
    }
    printf("Seeed cmd test over\r\n");
    return ESP_OK;
}

static esp_err_t bat_get_op(bat_op_t *self, int argc, char *argv[])
{
    
    bsp_battery_get_voltage();
    printf("Seeed cmd test over\r\n");
    return ESP_OK;
}

/* handle 'device' command */
static esp_err_t do_cmd_bat(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    bat_op_t cmd;

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
esp_err_t console_cmd_bat_register(void)
{
    esp_err_t ret;
    esp_console_cmd_t command = { .command = "bat", .help = "Command for bat operations\n For more info run 'bat help'", .func = &do_cmd_bat };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register device");
    }

    return ret;
}
