#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_spiffs.h"

#include "sensecap-watcher.h"

static const char *TAG = "main";

sscma_client_handle_t client = NULL;
sscma_client_flasher_handle_t flasher = NULL;
esp_io_expander_handle_t io_expander = NULL;

const char *model_info = "{\"uuid\":\"91331a9db811ed5cfb5cdba2e419e507\",\"name\":\"Gesture Detection\",\"version\":\"1.0.0\",\"category\":\"Object "
                         "Detection\",\"model_type\":\"TFLite\",\"algorithm\":\"Swift-YOLO power by SSCMA\",\"description\":\"The model is a Swift-YOLO model trained on the gesture detection "
                         "dataset.\",\"image\":\"https://files.seeedstudio.com/sscma/static/"
                         "detection_gesture.png\",\"classes\":[\"paper\",\"rock\",\"scissors\"],\"devices\":[\"we2\"],\"url\":\"https://files.seeedstudio.com/sscma/model_zoo/detection/gesture/"
                         "swift_yolo_1xb16_300e_coco_sha1_8d25b2b0be2a0ea38d3fe0aca5ce3891f7aa67c5_vela.tflite\",\"metrics\":{\"mAP(%)\":93,\"Inference(ms)\":{\"grove_vision_ai_we2\":47}},\"author\":"
                         "\"Seeed Studio\",\"checksum\":\"md5:91331a9db811ed5cfb5cdba2e419e507\",\"license\":\"MIT\"}";

void on_event(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx)
{
    // Note: reply is automatically recycled after exiting the function.

    char *img = NULL;
    int img_size = 0;
    if (sscma_utils_fetch_image_from_reply(reply, &img, &img_size) == ESP_OK)
    {
        ESP_LOGI(TAG, "image_size: %d\n", img_size);
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
                ESP_LOGI(TAG, "[box %d]: x=%d, y=%d, w=%d, h=%d, score=%d, target=%d\n", i, boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h, boxes[i].score, boxes[i].target);
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
                ESP_LOGI(TAG, "[class %d]: target=%d, score=%d\n", i, classes[i].target, classes[i].score);
            }
        }
        free(classes);
    }

    sscma_client_point_t *points = NULL;
    int point_count = 0;
    if (sscma_utils_fetch_points_from_reply(reply, &points, &point_count) == ESP_OK)
    {
        if (point_count > 0)
        {
            for (int i = 0; i < point_count; i++)
            {
                printf("[point %d]: x=%d, y=%d, z=%d, score=%d, target=%d\n", i, points[i].x, points[i].y, points[i].z, points[i].score, points[i].target);
            }
        }
        free(points);
    }

    sscma_client_keypoint_t *keypoints = NULL;
    int keypoints_count = 0;
    if (sscma_utils_fetch_keypoints_from_reply(reply, &keypoints, &keypoints_count) == ESP_OK)
    {
        if (keypoints_count > 0)
        {
            for (int i = 0; i < keypoints_count; i++)
            {
                printf("[keypoint %d]: [x=%d, y=%d, w=%d, h=%d, score=%d, target=%d]\n", i, keypoints[i].box.x, keypoints[i].box.y, keypoints[i].box.w, keypoints[i].box.h, keypoints[i].box.score,
                    keypoints[i].box.target);
                for (int j = 0; j < keypoints[i].points_num; j++)
                {
                    printf("\t [point %d]: [x=%d, y=%d, z=%d, score=%d, target=%d]\n", j, keypoints[i].points[j].x, keypoints[i].points[j].y, keypoints[i].points[j].z, keypoints[i].points[j].score,
                        keypoints[i].points[j].target);
                }
            }
        }
        free(keypoints);
    }
}

void on_log(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx)
{
    if (reply->len >= 100)
    {
        strcpy(&reply->data[100 - 4], "...");
    }

    ESP_LOGI(TAG, "log: %s\n", reply->data);
}

void app_main(void)
{
    io_expander = bsp_io_expander_init();
    assert(io_expander != NULL);
    client = bsp_sscma_client_init();
    assert(client != NULL);
    flasher = bsp_sscma_flasher_init();
    assert(flasher != NULL);

    bsp_spiffs_init_default();

    const sscma_client_callback_t callback = {
        .on_event = on_event,
        .on_log = on_log,
    };

    if (sscma_client_register_callback(client, &callback, NULL) != ESP_OK)
    {
        ESP_LOGI(TAG, "set callback failed\n");
        abort();
    }
    sscma_client_init(client);

    sscma_client_info_t *info;
    if (sscma_client_get_info(client, &info, true) == ESP_OK)
    {
        printf("ID: %s\n", (info->id != NULL) ? info->id : "NULL");
        printf("Name: %s\n", (info->name != NULL) ? info->name : "NULL");
        printf("Hardware Version: %s\n", (info->hw_ver != NULL) ? info->hw_ver : "NULL");
        printf("Software Version: %s\n", (info->sw_ver != NULL) ? info->sw_ver : "NULL");
        printf("Firmware Version: %s\n", (info->fw_ver != NULL) ? info->fw_ver : "NULL");
    }
    else
    {
        printf("get info failed\n");
    }
    int64_t start = esp_timer_get_time();
    ESP_LOGI(TAG, "Flash Firmware ...");
    if (sscma_client_ota_start(client, flasher, 0x000000) != ESP_OK)
    {
        ESP_LOGI(TAG, "sscma_client_ota_start failed\n");
    }
    else
    {
        ESP_LOGI(TAG, "sscma_client_ota_start success\n");
        FILE *f = fopen("/spiffs/firmware.img", "r");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "open firmware.img failed\n");
        }
        else
        {
            ESP_LOGI(TAG, "open firmware.img success\n");
            size_t len = 0;
            char buf[256] = { 0 };
            do
            {
                memset(buf, 0, sizeof(buf));
                if (fread(buf, 1, sizeof(buf), f) <= 0)
                {
                    printf("\n");
                    break;
                }
                else
                {
                    len += sizeof(buf);
                    if (sscma_client_ota_write(client, buf, sizeof(buf)) != ESP_OK)
                    {
                        ESP_LOGI(TAG, "sscma_client_ota_write failed\n");
                        break;
                    }
                }
            }
            while (true);
            fclose(f);
        }
        sscma_client_ota_finish(client);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "sscma_client_ota_finish success, take %lld us\n", esp_timer_get_time() - start);
    }

    if (sscma_client_get_info(client, &info, false) == ESP_OK)
    {
        printf("ID: %s\n", (info->id != NULL) ? info->id : "NULL");
        printf("Name: %s\n", (info->name != NULL) ? info->name : "NULL");
        printf("Hardware Version: %s\n", (info->hw_ver != NULL) ? info->hw_ver : "NULL");
        printf("Software Version: %s\n", (info->sw_ver != NULL) ? info->sw_ver : "NULL");
        printf("Firmware Version: %s\n", (info->fw_ver != NULL) ? info->fw_ver : "NULL");
    }
    else
    {
        printf("get info failed\n");
    }

    ESP_LOGI(TAG, "Flash gesture.tflite ...");
    start = esp_timer_get_time();
    if (sscma_client_ota_start(client, flasher, 0x800000) != ESP_OK)
    {
        ESP_LOGI(TAG, "sscma_client_ota_start failed\n");
    }
    else
    {
        ESP_LOGI(TAG, "sscma_client_ota_start success\n");
        FILE *f = fopen("/spiffs/gesture.tflite", "r");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "open gesture.tflite failed\n");
        }
        else
        {
            ESP_LOGI(TAG, "open gesture.tflite success\n");
            size_t len = 0;
            char buf[256] = { 0 };
            do
            {
                memset(buf, 0, sizeof(buf));
                if (fread(buf, 1, sizeof(buf), f) <= 0)
                {
                    printf("\n");
                    break;
                }
                else
                {
                    len += sizeof(buf);
                    if (sscma_client_ota_write(client, buf, sizeof(buf)) != ESP_OK)
                    {
                        ESP_LOGI(TAG, "sscma_client_ota_write failed\n");
                        break;
                    }
                }
            }
            while (true);
            fclose(f);
        }
        sscma_client_ota_finish(client);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "sscma_client_ota_finish success, take %lld us\n", esp_timer_get_time() - start);
    }

    // opt id
    // 0: 240 x 240
    // 1: 416 x 416
    // 2: 480 x 480
    // 2: 640 x 480
    sscma_client_set_sensor(client, 1, 1, true);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (sscma_client_set_model(client, 3) != ESP_OK)
    {
        ESP_LOGI(TAG, "set model failed\n");
        sscma_client_sample(client, -1);
    }
    else
    {
        ESP_LOGI(TAG, "set model success\n");
        if (sscma_client_set_model_info(client, (const char *)model_info) != ESP_OK)
        {
            ESP_LOGI(TAG, "set model info failed\n");
        }
        sscma_client_model_t *model;
        if (sscma_client_get_model(client, &model, false) == ESP_OK)
        {
            printf("ID: %d\n", model->id ? model->id : -1);
            printf("UUID: %s\n", model->uuid ? model->uuid : "N/A");
            printf("Name: %s\n", model->name ? model->name : "N/A");
            printf("Version: %s\n", model->ver ? model->ver : "N/A");
            printf("URL: %s\n", model->url ? model->url : "N/A");
            printf("Checksum: %s\n", model->checksum ? model->checksum : "N/A");
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
            printf("get model failed\n");
        }

        sscma_client_invoke(client, -1, false, true);
    }

    while (1)
    {
        ESP_LOGI(TAG, "free_heap_size = %ld\n", esp_get_free_heap_size());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
