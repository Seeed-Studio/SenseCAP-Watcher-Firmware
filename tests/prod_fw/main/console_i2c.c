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

#include "driver/i2c.h"

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"

#include "sensecap-watcher.h"

#include "console_i2c.h"

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_i2c", .plugin_regd_fn = &console_cmd_i2c_register };

typedef struct i2c_op_t
{
    char *name;
    esp_err_t (*operation)(struct i2c_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} i2c_op_t;

static esp_err_t i2c_help_op(i2c_op_t *self, int argc, char *argv[]);
static esp_err_t i2c_scan_op(i2c_op_t *self, int argc, char *argv[]);
static esp_err_t i2c_write_op(i2c_op_t *self, int argc, char *argv[]);
static esp_err_t i2c_read_op(i2c_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_i2c";

static i2c_op_t cmd_list[] = {
    { .name = "help", .operation = i2c_help_op, .arg_cnt = 2, .start_index = 1, .help = "i2c help: Prints the help text for all i2c commands" },
    { .name = "scan", .operation = i2c_scan_op, .arg_cnt = 3, .start_index = 1, .help = "i2c scan <bus number>: Scans for i2c devices on the given bus." },
    { .name = "write",
        .operation = i2c_read_op,
        .arg_cnt = 6,
        .start_index = 1,
        .help = "i2c write <bus number> <address> <length> <value>: Writes the given value to the given address on the given bus." },
    { .name = "read", .operation = i2c_write_op, .arg_cnt = 5, .start_index = 1, .help = "i2c read <bus number> <address> <length> : Reads the value from the given address on the given bus." },
};

static esp_err_t i2c_help_op(i2c_op_t *self, int argc, char *argv[])
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

static esp_err_t i2c_scan_op(i2c_op_t *self, int argc, char *argv[])
{
    if (argc < 3)
    {
        ESP_LOGE(TAG, "Usage:\n%s", cmd_list[0].help);
        return ESP_ERR_INVALID_ARG;
    }
    int bus = atoi(argv[2]);
    if (bus < 0 || bus > 2)
    {
        ESP_LOGE(TAG, "Invalid bus number");
        return ESP_ERR_INVALID_ARG;
    }

    bsp_i2c_detect(bus);

    return ESP_OK;
}

static esp_err_t i2c_write_op(i2c_op_t *self, int argc, char *argv[])
{
    if (argc < 6)
    {
        ESP_LOGE(TAG, "Usage:\n%s", cmd_list[1].help);
        return ESP_ERR_INVALID_ARG;
    }

    int bus = atoi(argv[2]);
    if (bus < 0 || bus > 2)
    {
        ESP_LOGE(TAG, "Invalid bus number");
        return ESP_ERR_INVALID_ARG;
    }

    int address = atoi(argv[3]);
    int length = atoi(argv[4]);
    static uint8_t value[256];

    for (int i = 0; i < length / sizeof(value); i++)
    {
        for (int j = 0; j < sizeof(value); j++)
        {
            value[j] = strtol(argv[5 + i * sizeof(value) + j], NULL, 16);
        }
        i2c_master_write_to_device(bus, address, value, sizeof(value), portMAX_DELAY);
    }

    for (int i = 0; i < length % sizeof(value); i++)
    {
        value[i] = strtol(argv[5 + (length / sizeof(value)) * sizeof(value) + i], NULL, 16);
    }

    i2c_master_write_to_device(bus, address, value, length % sizeof(value), 1000 / portTICK_PERIOD_MS);

    return ESP_OK;
}

static esp_err_t i2c_read_op(i2c_op_t *self, int argc, char *argv[])
{
    if (argc < 5)
    {
        ESP_LOGE(TAG, "Usage:\n%s", cmd_list[2].help);
        return ESP_ERR_INVALID_ARG;
    }

    int bus = atoi(argv[2]);
    if (bus < 0 || bus > 2)
    {
        ESP_LOGE(TAG, "Invalid bus number");
    }

    int address = atoi(argv[3]);
    int length = atoi(argv[4]);

    static uint8_t value[256];

    for (int i = 0; i < length / sizeof(value); i++)
    {
        i2c_master_read_from_device(bus, address, value, sizeof(value), portMAX_DELAY);
        for (int j = 0; j < sizeof(value); j++)
        {
            printf("%02x%c", value[j], (j + 1) % 16 == 0 ? '\n' : ' ');
        }
    }

    i2c_master_read_from_device(bus, address, value, length % sizeof(value), 1000 / portTICK_PERIOD_MS);
    for (int i = 0; i < length % sizeof(value); i++)
    {
        printf("%02x%c", value[i], (i + 1) % 16 == 0 ? '\n' : ' ');
    }

    return ESP_OK;
}

/* handle 'i2c' command */
static esp_err_t do_cmd_i2c(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    i2c_op_t cmd;

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
 * @brief Registers the i2c command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_i2c_register(void)
{
    esp_err_t ret;

    esp_console_cmd_t command = { .command = "i2c", .help = "Command for i2c operations\n For more info run 'i2c help'", .func = &do_cmd_i2c };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register i2c");
    }

    return ret;
}
