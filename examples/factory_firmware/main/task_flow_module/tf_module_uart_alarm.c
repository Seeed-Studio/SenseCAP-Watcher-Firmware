#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "cJSON.h"

#include "tf.h"
#include "tf_util.h"
#include "tf_module_uart_alarm.h"
#include "tf_module_util.h"
#include "util.h"


static const char *TAG = "tfm.uart_alarm";
static volatile atomic_int g_ins_cnt = ATOMIC_VAR_INIT(0);


static void __event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *p_event_data)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *)handler_args;
   
    uint8_t type = ((uint8_t *)p_event_data)[0];
    if( type !=  TF_DATA_TYPE_DUALIMAGE_WITH_AUDIO_TEXT) {
        ESP_LOGW(TAG, "unsupported type %d", type);
        tf_data_free(p_event_data);
        return;
    }

    tf_data_dualimage_with_audio_text_t *p_data = (tf_data_dualimage_with_audio_text_t*)p_event_data;
    uint16_t total_len = 0;
    uint8_t *buffer = NULL;
    cJSON *json = NULL;

    //prompt
    tf_info_t tf_info;
    tf_engine_info_get(&tf_info);
    const char *prompt = tf_info.p_tf_name;
    if (p_module_ins->output_format == 0) {
        //binary output
        uint16_t prompt_len = strlen(prompt);
        buffer = psram_calloc(1, prompt_len + 2);
        memcpy(buffer, &prompt_len, 2);
        memcpy(buffer + 2, prompt, prompt_len);
        total_len += prompt_len + 2;
    } else {
        //json output
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json, "prompt", cJSON_CreateString(prompt));
    }

    //big image
    if (p_module_ins->include_big_image) {
        ESP_LOGI(TAG, "include_big_image: %d", (int)p_module_ins->include_big_image);
        if (p_module_ins->output_format == 0) {
            //binary output
            uint16_t big_image_len = p_data->img_large.len;
            buffer = psram_realloc(buffer, total_len + big_image_len + 2);
            memcpy(buffer + total_len, &big_image_len, 2);
            memcpy(buffer + total_len + 2, p_data->img_large.p_buf, big_image_len);
            total_len += big_image_len + 2;
        } else {
            //json output
            cJSON_AddItemToObject(json, "big_image", cJSON_CreateString((char *)p_data->img_large.p_buf));
        }
    }

    //small image
    if (p_module_ins->include_small_image) {
        ESP_LOGI(TAG, "include_small_image: %d", (int)p_module_ins->include_small_image);
        if (p_module_ins->output_format == 0) {
            //binary output
            uint16_t small_image_len = p_data->img_small.len;
            buffer = psram_realloc(buffer, total_len + small_image_len + 2);
            memcpy(buffer + total_len, &small_image_len, 2);
            memcpy(buffer + total_len + 2, p_data->img_small.p_buf, small_image_len);
            total_len += small_image_len + 2;
        } else {
            //json output
            cJSON_AddItemToObject(json, "small_image", cJSON_CreateString((char *)p_data->img_small.p_buf));
        }
    }

#if 0
    //boxes
    if (p_module_ins->include_boxes) {
        if (p_module_ins->output_format == 0) {
            //binary output
        } else {
            //json output
            cJSON boxes = cJSON_CreateArray();
            for (size_t i = 0; i < count; i++)
            {
            }
            cJSON_AddItemToObject(json, "boxes", boxes);
        }
    }
#endif

    //output the packet
    const char *header = "SEEED";
    uart_write_bytes(UART_NUM_2, header, strlen(header));

    if (p_module_ins->output_format == 0) {
        uart_write_bytes(UART_NUM_2, buffer, total_len);
        free(buffer);
    } else {
        char *str = cJSON_PrintUnformatted(json);
        total_len = strlen(str);
        uart_write_bytes(UART_NUM_2, &total_len, 2);
        uart_write_bytes(UART_NUM_2, str, total_len);
        free(str);
        cJSON_Delete(json);
    }
}

/*************************************************************************
 * Interface implementation
 ************************************************************************/
static int __start(void *p_module)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *)p_module;
    return 0;
}

static int __stop(void *p_module)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *)p_module;
    return tf_event_handler_unregister(p_module_ins->input_evt_id, __event_handler);
}

static int __cfg(void *p_module, cJSON *p_json)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *)p_module;

    cJSON *output_format = cJSON_GetObjectItem(p_json, "output_format");
    if (output_format == NULL || !cJSON_IsNumber(output_format))
    {
        ESP_LOGE(TAG, "params output_format err, default 0 (binary output)");
        p_module_ins->output_format = 0;
    } else {
        ESP_LOGI(TAG, "params output_format=%d", output_format->valueint);
        p_module_ins->output_format = output_format->valueint;
    }

    cJSON *include_big_image = cJSON_GetObjectItem(p_json, "include_big_image");
    if (include_big_image == NULL || !cJSON_IsBool(include_big_image))
    {
        ESP_LOGE(TAG, "params include_big_image err, default false");
        p_module_ins->include_big_image = false;
    } else {
        ESP_LOGI(TAG, "params include_big_image=%s", cJSON_IsTrue(include_big_image)?"true":"false");
        p_module_ins->include_big_image = cJSON_IsTrue(include_big_image);
    }

    cJSON *include_small_image = cJSON_GetObjectItem(p_json, "include_small_image");
    if (include_small_image == NULL || !cJSON_IsBool(include_small_image))
    {
        ESP_LOGE(TAG, "params include_small_image err, default false");
        p_module_ins->include_small_image = false;
    } else {
        ESP_LOGI(TAG, "params include_small_image=%s", cJSON_IsTrue(include_small_image)?"true":"false");
        p_module_ins->include_small_image = cJSON_IsTrue(include_small_image);
    }

    cJSON *include_boxes = cJSON_GetObjectItem(p_json, "include_boxes");
    if (include_boxes == NULL || !cJSON_IsBool(include_boxes))
    {
        ESP_LOGE(TAG, "params include_boxes err, default false");
        p_module_ins->include_boxes = false;
    } else {
        ESP_LOGI(TAG, "params include_boxes=%s", cJSON_IsTrue(include_boxes)?"true":"false");
        p_module_ins->include_boxes = cJSON_IsTrue(include_boxes);
    }
    return 0;
}

static int __msgs_sub_set(void *p_module, int evt_id)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *)p_module;
    p_module_ins->input_evt_id = evt_id;
    return tf_event_handler_register(evt_id, __event_handler, p_module_ins);
}

static int __msgs_pub_set(void *p_module, int output_index, int *p_evt_id, int num)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *)p_module;
    if ( num > 0) {
        ESP_LOGW(TAG, "this module should have no output");
    }
    return 0;
}

const static struct tf_module_ops  __g_module_ops = {
    .start = __start,
    .stop = __stop,
    .cfg = __cfg,
    .msgs_sub_set = __msgs_sub_set,
    .msgs_pub_set = __msgs_pub_set
};

/*************************************************************************
 * API
 ************************************************************************/

tf_module_t *tf_module_uart_alarm_instance(void)
{
    tf_module_uart_alarm_t *p_module_ins = (tf_module_uart_alarm_t *) tf_malloc(sizeof(tf_module_uart_alarm_t));
    if (p_module_ins == NULL)
    {
        return NULL;
    }
    p_module_ins->module_base.p_module = p_module_ins;
    p_module_ins->module_base.ops = &__g_module_ops;

    if (atomic_fetch_add(&g_ins_cnt, 1) == 0) {
        // the 1st time instance, we should init the hardware
        esp_err_t ret;
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };
        const int buffer_size = 2 * 1024;
        ESP_GOTO_ON_ERROR(uart_param_config(UART_NUM_2, &uart_config), err, TAG, "uart_param_config failed");
        ESP_GOTO_ON_ERROR(uart_set_pin(UART_NUM_2, GPIO_NUM_19/*TX*/, GPIO_NUM_20/*RX*/, -1, -1), err, TAG, "uart_set_pin failed");
        ESP_GOTO_ON_ERROR(uart_driver_install(UART_NUM_2, buffer_size, buffer_size, 0, NULL, ESP_INTR_FLAG_SHARED), err, TAG, "uart_driver_install failed");
    }

    return &p_module_ins->module_base;

err:
    free(p_module_ins);
    return NULL;
}

void tf_module_uart_alarm_destroy(tf_module_t *p_module_base)
{
    if( atomic_fetch_sub(&g_ins_cnt, 1) <= 1 && p_module_base ) {
        free(p_module_base->p_module);
    }
}

const static struct tf_module_mgmt __g_module_management = {
    .tf_module_instance = tf_module_uart_alarm_instance,
    .tf_module_destroy = tf_module_uart_alarm_destroy,
};

esp_err_t tf_module_uart_alarm_register(void)
{
    return tf_module_register(TF_MODULE_UART_ALARM_NAME,
                              TF_MODULE_UART_ALARM_DESC,
                              TF_MODULE_UART_ALARM_VERSION,
                              &__g_module_management);
}