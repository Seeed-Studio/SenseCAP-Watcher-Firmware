#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <mbedtls/base64.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"

#include "sscma_client_io.h"
#include "sscma_client_ops.h"
#include "indoor_ai_camera.h"

#include "app_sscma_client.h"
#include "event_loops.h"
#include "data_defs.h"
#include "app_tasklist.h"
#include "app_sensecraft.h"

#include "view_image_preview.h"

static const char *TAG = "sscma-client";

static sscma_client_io_handle_t io = NULL;
static sscma_client_handle_t client = NULL;
static uint8_t alarm_flag = 0;

static SemaphoreHandle_t __g_event_sem;
static SemaphoreHandle_t   __g_data_mutex;
static struct view_data_image_invoke image_invoke;
static esp_timer_handle_t     alarm_timer_handle;

static int g_model_id = 1;

static void save_image_invoke(struct view_data_image_invoke *p_data)
{
    if( alarm_flag ) {
        return;
    }
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    image_invoke.boxes_cnt = 0; //p_data->boxes_cnt;
    memcpy(image_invoke.boxes, p_data->boxes, p_data->boxes_cnt * sizeof(struct view_data_boxes));
    memcpy(image_invoke.image.p_buf, p_data->image.p_buf, p_data->image.len);
    image_invoke.image.len = p_data->image.len;
    xSemaphoreGive(__g_data_mutex);
}
static void __timer_callback(void* arg)
{
    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_ALARM_OFF, NULL, 0, portMAX_DELAY);
}

static void __timer_start( int s)
{
    const esp_timer_create_args_t timer_args = {
            .callback = &__timer_callback,
            /* argument specified here will be passed to timer callback function */
            .arg = (void*) alarm_timer_handle,
            .name = "timer mode"
    };
    ESP_ERROR_CHECK( esp_timer_create(&timer_args, &alarm_timer_handle));
    
    ESP_ERROR_CHECK(esp_timer_start_once(alarm_timer_handle, (uint64_t) s * 1000000 ));
}


void on_event(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx)
{
    char *img = NULL;
    int img_size = 0;
    int32_t event_id = 0;
    cJSON *name = NULL;
    
    if( alarm_flag ) {
        return;
    }
    // printf("on_event: %s", reply->data);

    name = cJSON_GetObjectItem(reply->payload, "name");
    if( name != NULL  && name->valuestring != NULL ) {
       if(strcmp(name->valuestring, "SAMPLE") == 0) {
            event_id = VIEW_EVENT_IMAGE_640_480;  
       } else {
            event_id = VIEW_EVENT_IMAGE_240_240;
       }
    } else {
        return;
    }

    switch (event_id)
    {
        case VIEW_EVENT_IMAGE_240_240:
        {
            struct view_data_image_invoke invoke;
            sscma_client_box_t *boxes = NULL;
            int box_count = 0;

            if (sscma_utils_fetch_image_from_reply(reply, &img, &img_size) != ESP_OK) {
                break;
            }
            // ESP_LOGI(TAG, "image_size: %d\n", img_size);

            if (sscma_utils_fetch_boxes_from_reply(reply, &boxes, &box_count) != ESP_OK)
            {
                free(img);
                break;
            }

            invoke.image.p_buf = (uint8_t *)img;
            invoke.image.len = img_size;
            invoke.image.time = time(NULL);

            invoke.boxes_cnt = box_count > IMAGE_INVOKED_BOXES ? IMAGE_INVOKED_BOXES : box_count;
            
            if( invoke.boxes_cnt > 0) {
                for (size_t i = 0; i < invoke.boxes_cnt ; i++)
                {
                    // ESP_LOGI(TAG, "[box %d]: x=%d, y=%d, w=%d, h=%d, score=%d, target=%d\n", i, boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h, boxes[i].score, boxes[i].target);
                    invoke.boxes[i].x = boxes[i].x;
                    invoke.boxes[i].y = boxes[i].y;
                    invoke.boxes[i].w = boxes[i].w;
                    invoke.boxes[i].h = boxes[i].h;
                    invoke.boxes[i].score = boxes[i].score;
                    invoke.boxes[i].target = boxes[i].target;
                }
            }
            lvgl_port_lock(0);
            view_image_preview_flush(&invoke);
            lvgl_port_unlock();

            if( g_model_id == 1 && app_sensecraft_image_invoke_check(&invoke) ) {
                save_image_invoke(&invoke);
            }
            free(boxes);
            free(img);
            break;
        }
        case VIEW_EVENT_IMAGE_640_480:
        {
            struct view_data_image image;
            if (sscma_utils_fetch_image_from_reply(reply, &img, &img_size) != ESP_OK) {
                break;
            }
            ESP_LOGI(TAG, "640 480image_size: %d\n", img_size);
            image.p_buf = (uint8_t *)img;
            image.len = img_size;
            image.time = time(NULL);

            app_sensecraft_image_upload(&image);
            
            free(img);
            
            break;
        }
        default:
            break;
    }
}

void on_log(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx)
{
    if (reply->len >= 100)
    {
        strcpy(&reply->data[100 - 4], "...");
    }
    // Note: reply is automatically recycled after exiting the function.
    ESP_LOGI(TAG, "log: %s\n", reply->data);
}

void __init(void)
{
    esp_io_expander_handle_t io_expander = bsp_io_expander_init();

    const spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_SSCMA_SPI_SCLK,
        .mosi_io_num = EXAMPLE_SSCMA_SPI_MOSI,
        .miso_io_num = EXAMPLE_SSCMA_SPI_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4095,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_SSCMA_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO));

    const sscma_client_io_spi_config_t spi_io_config = {
        .sync_gpio_num = EXAMPLE_SSCMA_SPI_SYNC,
        .cs_gpio_num = EXAMPLE_SSCMA_SPI_CS,
        .pclk_hz = EXAMPLE_SSCMA_SPI_CLK_HZ,
        .spi_mode = 0,
        .wait_delay = 2,
        .user_ctx = NULL,
        .io_expander = io_expander,
        .flags.sync_use_expander = true,
    };

    sscma_client_new_io_spi_bus((sscma_client_spi_bus_handle_t)EXAMPLE_SSCMA_SPI_NUM, &spi_io_config, &io);

    sscma_client_config_t sscma_client_config = SSCMA_CLIENT_CONFIG_DEFAULT();
    sscma_client_config.reset_gpio_num = EXAMPLE_SSCMA_RESET;
    sscma_client_config.io_expander = io_expander;
    sscma_client_config.flags.reset_use_expander = true;

    ESP_ERROR_CHECK(sscma_client_new(io, &sscma_client_config, &client));
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
    sscma_client_set_model(client, g_model_id);
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
    sscma_client_model_t *model;
    if (sscma_client_get_model(client, &model, true) == ESP_OK)
    {
        printf("ID: %d\n", model->id ? model->id : -1);
        printf("UUID: %s\n", model->uuid ? model->uuid : "N/A");
        printf("Name: %s\n", model->name ? model->name : "N/A");
        printf("Version: %s\n", model->ver ? model->ver : "N/A");
        printf("Category: %s\n", model->category ? model->category : "N/A");
        printf("Algorithm: %s\n", model->algorithm ? model->algorithm : "N/A");
        printf("Description: %s\n", model->description ? model->description : "N/A");

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

        printf("Token: %s\n", model->token ? model->token : "N/A");
        printf("URL: %s\n", model->url ? model->url : "N/A");
        printf("Manufacturer: %s\n", model->manufacturer ? model->manufacturer : "N/A");
    }
    else
    {
        printf("get model failed\n");
    }
    sscma_client_break(client);
    /*
    Get available sensors
    Request: AT+SENSORS?\r
    Response:
    \r{
    "type": 0,
    "name": "SENSORS?",
    "code": 0,
    "data": [
        {
            "id": 1,
            "type": 1,
            "state": 1,
            "opt_id": 0,
            "opt_detail": "240x240 Auto",
            "opts": {
                "2": "640x480 Auto",
                "1": "480x480 Auto",
                "0": "240x240 Auto"
            }
        }
    ]
    }\n
    */
    sscma_client_set_sensor(client, 1, 0, true);
    if (sscma_client_invoke(client, -1, false, true) != ESP_OK)
    {
        ESP_LOGI(TAG, "sample failed\n");
    }
    if (sscma_client_set_iou_threshold(client, 40) != ESP_OK)
    {
        printf("set iou threshold failed\n");
    }

    if (sscma_client_set_confidence_threshold(client, 50) != ESP_OK)
    {
        printf("set confidence threshold failed\n");
    }
    int iou_threshold = 0;
    if (sscma_client_get_iou_threshold(client, &iou_threshold) == ESP_OK)
    {
        printf("iou threshold: %d\n", iou_threshold);
    }
    int confidence_threshold = 0;
    if (sscma_client_get_confidence_threshold(client, &confidence_threshold) == ESP_OK)
    {
        printf("confidence threshold: %d\n", confidence_threshold);
    }
 
}

void __app_sscma_client_task(void *p_arg)
{
    ESP_LOGI(TAG, "start");
    while(1) {
        xSemaphoreTake(__g_event_sem, pdMS_TO_TICKS(5000));
    }
}

static void __view_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    switch (id)
    {
        case VIEW_EVENT_IMAGE_240_240_REQ: {
            ESP_LOGI(TAG, "event: VIEW_EVENT_IMAGE_240_240_REQ");
            sscma_client_break(client);
            sscma_client_set_sensor(client, 1, 0, true);
            if (sscma_client_invoke(client, -1, false, true) != ESP_OK)
            {
                ESP_LOGI(TAG, "sample failed\n");
            } 
            break;
        }
        case VIEW_EVENT_IMAGE_640_480_REQ: {
            ESP_LOGI(TAG, "event: VIEW_EVENT_IMAGE_640_480_REQ");
            sscma_client_break(client);
            sscma_client_set_sensor(client, 1, 2, true);
            if (sscma_client_sample(client, 1) != ESP_OK)
            {
                ESP_LOGI(TAG, "sample failed\n");
            }
            break;
        }

        case VIEW_EVENT_ALARM_ON:
        {
            ESP_LOGI(TAG, "event: VIEW_EVENT_ALARM_ON");

            sscma_client_break(client);

            xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
            alarm_flag =  true;
            xSemaphoreGive(__g_data_mutex);

            int len = 0;
            char *p_img = NULL;
            struct view_data_image_invoke invoke;


            len = tasklist_image_get(&p_img);
            invoke.boxes_cnt=0;
            invoke.image.len= len;
            invoke.image.p_buf = (uint8_t *)p_img;

            
            xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
            lvgl_port_lock(0);
            
            if( len > 0 ) {
                ESP_LOGI(TAG, "display tasklist image");
                view_image_preview_flush(&invoke);
                free(p_img);
            } else {
                ESP_LOGI(TAG, "display cache image");
                view_image_preview_flush(&image_invoke); 
            }
            view_image_preview_flush(&image_invoke);
            lvgl_port_unlock();
            xSemaphoreGive(__g_data_mutex);

            __timer_start(20);

            break;
        }
        case VIEW_EVENT_ALARM_OFF:
        {
            ESP_LOGI(TAG, "event: VIEW_EVENT_ALARM_OFF");

            xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
            alarm_flag =  false;
            xSemaphoreGive(__g_data_mutex);

            sscma_client_break(client);
            sscma_client_set_sensor(client, 1, 0, true);
            if (sscma_client_invoke(client, -1, false, true) != ESP_OK)
            {
                ESP_LOGI(TAG, "sample failed\n");
            }

            break;
        }
        case VIEW_EVENT_IMAGE_MODEL:
        {
            ESP_LOGI(TAG, "event: VIEW_EVENT_IMAGE_MODEL");
            int *p_model =  (int * )event_data;
            g_model_id = *p_model;
            sscma_client_set_model(client, *p_model);
            sscma_client_break(client);


            sscma_client_model_t *model;
            if (sscma_client_get_model(client, &model, true) == ESP_OK)
            {
                printf("ID: %d\n", model->id ? model->id : -1);
                printf("UUID: %s\n", model->uuid ? model->uuid : "N/A");
                printf("Name: %s\n", model->name ? model->name : "N/A");
                printf("Version: %s\n", model->ver ? model->ver : "N/A");
                printf("Category: %s\n", model->category ? model->category : "N/A");
                printf("Algorithm: %s\n", model->algorithm ? model->algorithm : "N/A");
                printf("Description: %s\n", model->description ? model->description : "N/A");

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

                printf("Token: %s\n", model->token ? model->token : "N/A");
                printf("URL: %s\n", model->url ? model->url : "N/A");
                printf("Manufacturer: %s\n", model->manufacturer ? model->manufacturer : "N/A");
            }
            else
            {
                printf("get model failed\n");
            }  
            sscma_client_set_sensor(client, 1, 0, true);
            if (sscma_client_invoke(client, -1, false, true) != ESP_OK)
            {
                ESP_LOGI(TAG, "sample failed\n");
            }

            if (sscma_client_set_iou_threshold(client, 40) != ESP_OK)
            {
                printf("set iou threshold failed\n");
            }

            if (sscma_client_set_confidence_threshold(client, 50) != ESP_OK)
            {
                printf("set confidence threshold failed\n");
            }
            int iou_threshold = 0;
            if (sscma_client_get_iou_threshold(client, &iou_threshold) == ESP_OK)
            {
                printf("iou threshold: %d\n", iou_threshold);
            }
            int confidence_threshold = 0;
            if (sscma_client_get_confidence_threshold(client, &confidence_threshold) == ESP_OK)
            {
                printf("confidence threshold: %d\n", confidence_threshold);
            }

            break;
        }
    default:
        break;
    }
}


int app_sscma_client_init()
{
    memset(&image_invoke, 0, sizeof(image_invoke));
    image_invoke.image.p_buf = (uint8_t *)malloc(IMAGE_640_480_BUF_SIZE);
    assert(image_invoke.image.p_buf);

    __init();
    

    __g_data_mutex = xSemaphoreCreateMutex();

    // __g_event_sem = xSemaphoreCreateBinary();
    // xTaskCreate(&__app_sscma_client_task, "app_sscma_client_task", 1024 * 5, NULL, 10, NULL);

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_IMAGE_240_240_REQ, 
                                                            __view_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_IMAGE_640_480_REQ, 
                                                            __view_event_handler, NULL, NULL)); 

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_ALARM_ON, 
                                                            __view_event_handler, NULL, NULL));   

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_ALARM_OFF, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_IMAGE_MODEL, 
                                                            __view_event_handler, NULL, NULL));                                                                                                         
    return 0;
}

