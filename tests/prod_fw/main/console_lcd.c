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

#include "lv_demos.h"

#include "console_lcd.h"

static lv_disp_t *g_lvgl_disp = NULL;
static lv_indev_t *g_lvgl_tp = NULL;
static lv_obj_t *g_canvas = NULL;
static lv_color_t *g_cbuf = NULL;

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_lcd", .plugin_regd_fn = &console_cmd_lcd_register };

typedef struct lcd_op_t
{
    char *name;
    esp_err_t (*operation)(struct lcd_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} lcd_op_t;

static esp_err_t lcd_help_op(lcd_op_t *self, int argc, char *argv[]);
static esp_err_t lcd_brightness_op(lcd_op_t *self, int argc, char *argv[]);
static esp_err_t lcd_fill_op(lcd_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_lcd";

static lcd_op_t cmd_list[] = {
    { .name = "help", .operation = lcd_help_op, .arg_cnt = 2, .start_index = 1, .help = "lcd help: Prints the help text for all lcd commands" },
    { .name = "brightness", .operation = lcd_brightness_op, .arg_cnt = 3, .start_index = 1, .help = "lcd brightness <brightness>: Sets the brightness of the LCD" },
    { .name = "fill", .operation = lcd_fill_op, .arg_cnt = 3, .start_index = 1, .help = "lcd fill <value>: Fills the LCD with the given value" },
};

static esp_err_t lcd_help_op(lcd_op_t *self, int argc, char *argv[])
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

static esp_err_t lcd_brightness_op(lcd_op_t *self, int argc, char *argv[])
{
    if (argc < 3)
    {
        ESP_LOGE(TAG, "Usage:\n%s", cmd_list[0].help);
        return ESP_ERR_INVALID_ARG;
    }

    int brightness = atoi(argv[2]);

    if (brightness < 0 || brightness > 100)
    {
        ESP_LOGE(TAG, "Invalid brightness: %d (0 - 100)", brightness);
        return ESP_ERR_INVALID_ARG;
    }

    bsp_lcd_brightness_set(brightness);

    return ESP_OK;
}

static void tp_task(void *arg)
{
    lv_point_t point_last = { 0, 0 };
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    while (1)
    {
        if (lvgl_port_lock(0))
        {
            lv_indev_get_point(g_lvgl_tp, &point_last);
            lv_canvas_draw_rect(g_canvas, point_last.x, point_last.y, 10, 10, &rect_dsc);
            lvgl_port_unlock();
            vTaskDelay(17 / portTICK_PERIOD_MS);
        }
    }
}

static esp_err_t lcd_fill_op(lcd_op_t *self, int argc, char *argv[])
{
    if (argc < 3)
    {
        ESP_LOGE(TAG, "Usage:\n%s", cmd_list[1].help);
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t value = strtol(argv[2], NULL, 16);

    if (lvgl_port_lock(0))
    {
        lv_canvas_fill_bg(g_canvas, lv_color_hex(value), LV_OPA_COVER);

        lvgl_port_unlock();
    }

    return ESP_OK;
}

/* handle 'lcd' command */
static esp_err_t do_cmd_lcd(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    lcd_op_t cmd;

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
 * @brief Registers the lcd command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_lcd_register(void)
{
    esp_err_t ret;

    bsp_io_expander_init();

    g_lvgl_disp = bsp_lvgl_init();
    assert(g_lvgl_disp != NULL);

    while (1)
    {
        g_lvgl_tp = lv_indev_get_next(g_lvgl_tp);
        if (g_lvgl_tp->driver->type == LV_INDEV_TYPE_POINTER)
        {
            break;
        }
    }
    assert(g_lvgl_tp != NULL);

    g_cbuf = heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(DRV_LCD_H_RES, DRV_LCD_V_RES), MALLOC_CAP_SPIRAM);
    g_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(g_canvas, g_cbuf, DRV_LCD_H_RES, DRV_LCD_V_RES, LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(g_canvas);
    lv_canvas_fill_bg(g_canvas, lv_color_black(), LV_OPA_COVER);

    xTaskCreate(&tp_task, "tp_task", 2048, NULL, 5, NULL);

    esp_console_cmd_t command = { .command = "lcd", .help = "Command for lcd operations\n For more info run 'lcd help'", .func = &do_cmd_lcd };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register lcd");
    }

    return ret;
}
