#include "tf_parse.h"
#include <string.h>
#include "tf_util.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "tf.parse";

static void __module_item_free(tf_module_item_t *p_item, int num)
{
    for (int i = 0; i < num; i++)
    {
        int output_port_num = p_item[i].output_port_num;
        for (int j = 0; j < output_port_num; j++)
        {
            if (p_item[i].p_wires[j].p_evt_id != NULL)
            {
                tf_free(p_item[i].p_wires[j].p_evt_id);
            }
        }
        if (p_item[i].p_wires != NULL)
        {
            tf_free(p_item[i].p_wires);
        }
    }
}

int tf_parse_json_with_length(const char *p_str, size_t len,
                              cJSON **pp_json_root,
                              tf_module_item_t **pp_head)
{
    esp_err_t ret = ESP_OK;

    *pp_json_root = NULL;
    *pp_head = NULL;

    cJSON *p_json_root = NULL;
    cJSON *p_tasklist = NULL;

    tf_module_item_t *p_list_head = NULL;

    int module_item_num = 0;

    p_json_root = cJSON_ParseWithLength(p_str, len);
    ESP_GOTO_ON_FALSE(p_json_root, ESP_ERR_INVALID_ARG, err, TAG, "json parse failed");

    p_tasklist = cJSON_GetObjectItem(p_json_root, "tl");
    if (p_tasklist == NULL || !cJSON_IsArray(p_tasklist))
    {
        ESP_LOGE(TAG, "tl is not array");
        goto err;
    }

    module_item_num = cJSON_GetArraySize(p_tasklist);
    ESP_GOTO_ON_FALSE(module_item_num, ESP_ERR_INVALID_ARG, err, TAG, "tasklist is empty");

    p_list_head = (tf_module_item_t *)tf_malloc(sizeof(tf_module_item_t) * module_item_num);
    ESP_GOTO_ON_FALSE(p_list_head, ESP_ERR_NO_MEM, err, TAG, "malloc failed");

    memset((void *)p_list_head, 0, sizeof(tf_module_item_t) * module_item_num);

    for (int i = 0; i < module_item_num; i++)
    {
        cJSON *p_id = NULL;
        cJSON *p_type = NULL;
        cJSON *p_index = NULL;
        cJSON *p_params = NULL;
        cJSON *p_wires = NULL;
        int output_port_num = 0;

        cJSON *p_item = cJSON_GetArrayItem(p_tasklist, i);
        if (p_item == NULL || !cJSON_IsObject(p_item))
        {
            ESP_LOGE(TAG, "tasklist[%d] is not object", i);
            goto err;
        }

        p_id = cJSON_GetObjectItem(p_item, "id");
        if (p_id == NULL || !cJSON_IsNumber(p_id))
        {
            ESP_LOGE(TAG, "tasklist[%d] id is not number", i);
            goto err;
        }

        p_type = cJSON_GetObjectItem(p_item, "type");
        if (p_type == NULL || !cJSON_IsString(p_type))
        {
            ESP_LOGE(TAG, "tasklist[%d] type is not string", i);
            goto err;
        }

        p_index = cJSON_GetObjectItem(p_item, "index");
        if (p_index == NULL || !cJSON_IsNumber(p_index))
        {
            ESP_LOGE(TAG, "tasklist[%d] index is not number", i);
            goto err;
        }

        p_params = cJSON_GetObjectItem(p_item, "params");
        if (p_params == NULL || !cJSON_IsObject(p_params))
        {
            ESP_LOGE(TAG, "tasklist[%d] params is not object", i);
            goto err;
        }

        p_wires = cJSON_GetObjectItem(p_item, "wires");
        if (p_wires == NULL || !cJSON_IsArray(p_wires))
        {
            ESP_LOGE(TAG, "tasklist[%d] wires is not array", i);
            goto err;
        }

        p_list_head[i].id = p_id->valueint;
        p_list_head[i].p_name = p_type->valuestring;
        p_list_head[i].index = p_index->valueint;
        p_list_head[i].p_params = p_params;

        // handle wires parse
        output_port_num = cJSON_GetArraySize(p_wires);
        if (output_port_num)
        {
            p_list_head[i].p_wires = (struct tf_module_wires *)tf_malloc(sizeof(struct tf_module_wires) * output_port_num);
            ESP_GOTO_ON_FALSE(p_list_head[i].p_wires, ESP_ERR_NO_MEM, err, TAG, "malloc failed");
            p_list_head[i].output_port_num = output_port_num;

            for (int m = 0; m < output_port_num; m++)
            {
                int evt_id_num = 0;
                cJSON *p_wires_item_tmp = cJSON_GetArrayItem(p_wires, m);
                if (p_wires_item_tmp == NULL || !cJSON_IsArray(p_wires_item_tmp))
                {
                    ESP_LOGE(TAG, "tasklist[%d] wires[%d] is not array", i, m);
                    goto err;
                }
                evt_id_num = cJSON_GetArraySize(p_wires_item_tmp);
                if (evt_id_num <= 0)
                {
                    ESP_LOGE(TAG, "tasklist[%d] wires[%d] is empty", i, m);
                    goto err;
                }
                p_list_head[i].p_wires[m].p_evt_id = (int *)tf_malloc(sizeof(int) * evt_id_num);
                ESP_GOTO_ON_FALSE(p_list_head[i].p_wires[m].p_evt_id, ESP_ERR_NO_MEM, err, TAG, "malloc failed");
                memset((void *)p_list_head[i].p_wires[m].p_evt_id, 0, sizeof(int) * evt_id_num);
                for (int n = 0; n < evt_id_num; n++)
                {
                    p_list_head[i].p_wires[m].p_evt_id[n] = cJSON_GetArrayItem(p_wires_item_tmp, n)->valueint;
                }
                p_list_head[i].p_wires[m].num = evt_id_num;
            }
        }
    }
    *pp_json_root = p_json_root;
    *pp_head = p_list_head;

    return module_item_num;

err:
    if (p_json_root != NULL)
    {
        cJSON_Delete(p_json_root);
    }
    if (p_list_head != NULL)
    {
        __module_item_free(p_list_head, module_item_num);
        free(p_list_head);
    }
    return -1;
}

int tf_parse_json(const char *p_str,
                  cJSON **pp_json_root,
                  tf_module_item_t **pp_head)
{
    return tf_parse_json_with_length(p_str, strlen(p_str), pp_json_root, pp_head);
}

void tf_parse_free(cJSON *p_json_root, tf_module_item_t *p_head, int num)
{
    if (p_json_root)
    {
        cJSON_Delete(p_json_root);
    }
    if (p_head != NULL)
    {
        __module_item_free(p_head, num);
        tf_free(p_head);
    }
}