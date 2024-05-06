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

#include "driver/gpio.h"

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"

#include "sensecap-watcher.h"

#include "console_gpio.h"

static esp_io_expander_handle_t g_io_expander = NULL;

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_gpio", .plugin_regd_fn = &console_cmd_gpio_register };

typedef struct gpio_op_t
{
    char *name;
    esp_err_t (*operation)(struct gpio_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} gpio_op_t;

static esp_err_t gpio_help_op(gpio_op_t *self, int argc, char *argv[]);
static esp_err_t gpio_set_op(gpio_op_t *self, int argc, char *argv[]);
static esp_err_t gpio_get_op(gpio_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_gpio";

static gpio_op_t cmd_list[] = {
    { .name = "help", .operation = gpio_help_op, .arg_cnt = 2, .start_index = 1, .help = "gpio help: Prints the help text for all gpio commands" },
    { .name = "set", .operation = gpio_set_op, .arg_cnt = 4, .start_index = 1, .help = "gpio set <pin number> <value>: Sets the value of the given pin." },
    { .name = "get", .operation = gpio_get_op, .arg_cnt = 3, .start_index = 1, .help = "gpio get <pin number>: Gets the value of the given pin." },
};

static esp_err_t gpio_help_op(gpio_op_t *self, int argc, char *argv[])
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

static esp_err_t gpio_set_op(gpio_op_t *self, int argc, char *argv[])
{
    if (argc < 4)
    {
        ESP_LOGE(TAG, "Usage:\n%s", self->help);
        return ESP_ERR_INVALID_ARG;
    }

    int pin = atoi(argv[2]);
    int value = atoi(argv[3]);

    if (pin <= -16 || pin >= GPIO_NUM_MAX)
    {
        ESP_LOGE(TAG, "Invalid pin number: %d (-16 - %d)", pin, GPIO_NUM_MAX - 1);
        return ESP_ERR_INVALID_ARG;
    }

    if (value != 0 && value != 1)
    {
        ESP_LOGE(TAG, "Invalid value: %d (0 or 1)", value);
        return ESP_ERR_INVALID_ARG;
    }

    if (pin >= 0)
    {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << pin;
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        gpio_set_level(pin, value);
    }
    else
    {
        pin = (1 << (-pin - 1));
        bsp_exp_io_set_level(pin, value);
    }

    return ESP_OK;
}

static esp_err_t gpio_get_op(gpio_op_t *self, int argc, char *argv[])
{
    if (argc < 3)
    {
        ESP_LOGE(TAG, "Usage:\n%s", self->help);
        return ESP_ERR_INVALID_ARG;
    }

    int pin = atoi(argv[2]);
    int value = 0;

    if (pin <= -16 || pin >= GPIO_NUM_MAX)
    {
        ESP_LOGE(TAG, "Invalid pin number: %d (-16 - %d)", pin, GPIO_NUM_MAX - 1);
        return ESP_ERR_INVALID_ARG;
    }

    if (pin >= 0)
    {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = 1ULL << pin;
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        value = gpio_get_level(pin);
    }
    else
    {
        pin = (1 << (-pin - 1));
        value = bsp_exp_io_get_level(pin);
    }

    printf("%d\n", value);

    return ESP_OK;
}

/* handle 'gpio' command */
static esp_err_t do_cmd_gpio(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    gpio_op_t cmd;

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
 * @brief Registers the gpio command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_gpio_register(void)
{
    esp_err_t ret;

    g_io_expander = bsp_io_expander_init();

    assert(g_io_expander != NULL);

    esp_console_cmd_t command = { .command = "gpio", .help = "Command for gpio operations\n For more info run 'gpio help'", .func = &do_cmd_gpio };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register gpio");
    }

    return ret;
}
