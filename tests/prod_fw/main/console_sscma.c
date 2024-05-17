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

#include "mbedtls/base64.h"

#include "sensecap-watcher.h"

#include "esp_jpeg_dec.h"

#include "console_sscma.h"

#define IMG_WIDTH  416
#define IMG_HEIGHT 416

static lv_disp_t *g_lvgl_disp = NULL;
static sscma_client_handle_t client = NULL;
static lv_obj_t *image;
static uint8_t jpeg_buf[48 * 1024] = { 0 };

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = { .name = "console_cmd_lcd", .plugin_regd_fn = &console_cmd_sscma_register };

typedef struct sscma_op_t
{
    char *name;
    esp_err_t (*operation)(struct sscma_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} sscma_op_t;

static esp_err_t sscma_help_op(sscma_op_t *self, int argc, char *argv[]);
static esp_err_t sscma_device_op(sscma_op_t *self, int argc, char *argv[]);
static esp_err_t sscma_model_op(sscma_op_t *self, int argc, char *argv[]);
static esp_err_t sscma_preview_op(sscma_op_t *self, int argc, char *argv[]);
static esp_err_t sscma_invoke_op(sscma_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_sscma";

static sscma_op_t cmd_list[] = {
    { .name = "help", .operation = sscma_help_op, .arg_cnt = 2, .start_index = 1, .help = "sscma help: Prints the help text for all sscma commands" },
    { .name = "device", .operation = sscma_device_op, .arg_cnt = 2, .start_index = 1, .help = "sscma device: Prints the device information" },
    { .name = "model", .operation = sscma_model_op, .arg_cnt = 3, .start_index = 1, .help = "sscma model <model>: Sets the model" },
    { .name = "preview", .operation = sscma_preview_op, .arg_cnt = 3, .start_index = 1, .help = "sscma preview <times>: Starts the preview" },
    { .name = "invoke", .operation = sscma_invoke_op, .arg_cnt = 3, .start_index = 1, .help = "sscma invoke <model>: Invokes the model" },
};

static lv_img_dsc_t img_dsc = {
    .header.always_zero = 0,
    .header.w = IMG_WIDTH,
    .header.h = IMG_HEIGHT,
    .data_size = IMG_WIDTH * IMG_HEIGHT * LV_COLOR_DEPTH / 8,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};
static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    esp_err_t ret = ESP_OK;
    // Generate default configuration
    jpeg_dec_config_t config = { .output_type = JPEG_RAW_TYPE_RGB565_BE, .rotate = JPEG_ROTATE_0D };

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    jpeg_dec = jpeg_dec_open(&config);

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL)
    {
        return ESP_FAIL;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL)
    {
        return ESP_FAIL;
    }

    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);

    if (ret < 0)
    {
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;
    int inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + inbuf_consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0)
    {
        goto _exit;
    }

_exit:
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return ret;
}

void display_one_image(lv_obj_t *image, const unsigned char *p_data)
{
    if (!p_data)
        return;

    size_t str_len = strlen((const char *)p_data);
    size_t output_len = 0;

    int decode_ret = mbedtls_base64_decode(jpeg_buf, sizeof(jpeg_buf), &output_len, p_data, str_len);

    if (decode_ret == 0)
    {
        if (img_dsc.data == NULL)
        {
            img_dsc.data = heap_caps_aligned_alloc(16, img_dsc.data_size, MALLOC_CAP_SPIRAM);
        }
        int ret = esp_jpeg_decoder_one_picture(jpeg_buf, output_len, (uint8_t *)img_dsc.data);
        if (ret == ESP_OK)
        {
            lv_img_set_src(image, &img_dsc);
        }
    }

    return;
}

void on_event(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx)
{
    // Note: reply is automatically recycled after exiting the function.

    char *img = NULL;
    int img_size = 0;

    if (sscma_utils_fetch_image_from_reply(reply, &img, &img_size) == ESP_OK)
    {
        if (lvgl_port_lock(0))
        {
            display_one_image(image, (const unsigned char *)img);
            lvgl_port_unlock();
        }

        free(img);
    }
    sscma_client_box_t *boxes = NULL;
    int box_count = 0;
    if (sscma_utils_fetch_boxes_from_reply(reply, &boxes, &box_count) == ESP_OK)
    {
        if (box_count > 0)
        {
            for (int i = 0; i < box_count; i++)
            {
                printf("[box %d]: x=%d, y=%d, w=%d, h=%d, score=%d, target=%d\n", i, boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h, boxes[i].score, boxes[i].target);
            }
        }
        free(boxes);
    }

    sscma_client_class_t *classes = NULL;
    int class_count = 0;
    if (sscma_utils_fetch_classes_from_reply(reply, &classes, &class_count) == ESP_OK)
    {
        if (class_count > 0)
        {
            for (int i = 0; i < class_count; i++)
            {
                printf("[class %d]: target=%d, score=%d\n", i, classes[i].target, classes[i].score);
            }
        }
        free(classes);
    }
    return;
}

void on_log(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx)
{
    if (reply->len >= 100)
    {
        strcpy(&reply->data[100 - 4], "...");
    }
    // Note: reply is automatically recycled after exiting the function.
    printf("log: %s\n", reply->data);
}

static esp_err_t sscma_help_op(sscma_op_t *self, int argc, char *argv[])
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

static esp_err_t sscma_device_op(sscma_op_t *self, int argc, char *argv[])
{
    sscma_client_info_t *info;

    if (sscma_client_get_info(client, &info, true) == ESP_OK)
    {
        printf("ID: %s\n", (info->id != NULL) ? info->id : "NULL");
        printf("Name: %s\n", (info->name != NULL) ? info->name : "NULL");
        printf("Hardware Version: %s\n", (info->hw_ver != NULL) ? info->hw_ver : "NULL");
        printf("Software Version: %s\n", (info->sw_ver != NULL) ? info->sw_ver : "NULL");
        printf("Firmware Version: %s\n", (info->fw_ver != NULL) ? info->fw_ver : "NULL");
    }

    return ESP_OK;
}

static esp_err_t sscma_model_op(sscma_op_t *self, int argc, char *argv[])
{
    sscma_client_model_t *model;
    int id = 0;

    if (argc)
    {
        id = atoi(argv[2]);
    }

    if (sscma_client_set_model(client, id) != ESP_OK)
    {
        printf("Set model failed\n");
        return ESP_FAIL;
    }

    if (sscma_client_get_model(client, &model, true) == ESP_OK)
    {
        printf("ID: %d\n", model->id ? model->id : -1);
        printf("UUID: %s\n", model->uuid ? model->uuid : "N/A");
        printf("Name: %s\n", model->name ? model->name : "N/A");
        printf("Version: %s\n", model->ver ? model->ver : "N/A");
        printf("URL: %s\n", model->url ? model->url : "N/A");
        printf("Classes:\n");
        if (model->classes[0] != NULL)
        {
            for (int i = 0; model->classes[i] != NULL; i++)
            {
                printf("  - %s\n", model->classes[i]);
            }
        }
        else
        {
            printf("  N/A\n");
        }
    }
    else
    {
        printf("Get model failed\n");
    }

    return ESP_OK;
}

static esp_err_t sscma_preview_op(sscma_op_t *self, int argc, char *argv[])
{
    int times = 0;
    if (argc)
    {
        times = atoi(argv[2]);
    }
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    image = lv_img_create(lv_scr_act());
    lv_obj_set_align(image, LV_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(image, LV_SCROLLBAR_MODE_OFF);

    sscma_client_set_sensor(client, 1, 1, true);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (sscma_client_sample(client, times) != ESP_OK)
    {
        printf("Preview failed\n");
    }
    return ESP_OK;
}

static esp_err_t sscma_invoke_op(sscma_op_t *self, int argc, char *argv[])
{
    int times = 0;
    if (argc)
    {
        times = atoi(argv[2]);
    }
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    image = lv_img_create(lv_scr_act());
    lv_obj_set_align(image, LV_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(image, LV_SCROLLBAR_MODE_OFF);

    sscma_client_set_sensor(client, 1, 1, true);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (sscma_client_invoke(client, times, false, true) != ESP_OK)
    {
        printf("Invoke failed\n");
    }
    return ESP_OK;
}

/* handle 'sscma' command */
static esp_err_t do_cmd_sscma(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    sscma_op_t cmd;

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
 * @brief Registers the sscma command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_sscma_register(void)
{
    esp_err_t ret;
    bsp_io_expander_init();
    g_lvgl_disp = bsp_lvgl_get_disp();
    if (g_lvgl_disp == NULL)
    {
        g_lvgl_disp = bsp_lvgl_init();
    }
    assert(g_lvgl_disp != NULL);
    client = bsp_sscma_client_init();
    assert(client != NULL);

    image = lv_img_create(lv_scr_act());
    lv_obj_set_align(image, LV_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(image, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_SCROLLABLE);

    const sscma_client_callback_t callback = {
        .on_event = on_event,
        .on_log = on_log,
    };

    if (sscma_client_register_callback(client, &callback, NULL) != ESP_OK)
    {
        printf("set callback failed\n");
        abort();
    }

    sscma_client_init(client);

    esp_console_cmd_t command = { .command = "sscma", .help = "Command for sscma operations\n For more info run 'sscma help'", .func = &do_cmd_sscma };

    ret = esp_console_cmd_register(&command);
    if (ret)
    {
        ESP_LOGE(TAG, "Unable to register sscma");
    }

    return ret;
}
