#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "sdkconfig.h"
#if CONFIG_SSCMA_ENABLE_DEBUG_LOG
// The local log level must be defined before including esp_log.h
// Set the maximum log level for this source file
#define LOG_SSCMA_LEVEL ESP_LOG_DEBUG
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/list.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "sscma_client_types.h"
#include "sscma_client_io.h"
#include "sscma_client_commands.h"
#include "sscma_client_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "sscma_client";

const int error_map[] = {
    ESP_OK,
    ESP_ERR_NOT_FINISHED,
    ESP_FAIL,
    ESP_ERR_TIMEOUT,
    ESP_ERR_INVALID_RESPONSE,
    ESP_ERR_INVALID_ARG,
    ESP_ERR_NO_MEM,
    ESP_ERR_INVALID_STATE,
    ESP_ERR_NOT_SUPPORTED,
    ESP_FAIL,
};

#define SSCMA_CLIENT_CMD_ERROR_CODE(err) (error_map[(err & 0x0F) > (CMD_EUNKNOWN - 1) ? (CMD_EUNKNOWN - 1) : (err & 0x0F)])

static void fetch_string_common(cJSON *object, cJSON *field, char **target)
{
    if (field == NULL || !cJSON_IsString(field))
    {
        *target = NULL;
        return;
    }

    if (*target != NULL)
    {
        free(*target);
        *target = NULL;
    }

    *target = strdup(field->valuestring);
}

static inline void fetch_string_from_object(cJSON *object, const char *field_name, char **target)
{
    if (object == NULL || !cJSON_IsObject(object))
    {
        *target = NULL;
        return;
    }

    cJSON *field = cJSON_GetObjectItem(object, field_name);
    fetch_string_common(object, field, target);
}

static inline void fetch_string_from_array(cJSON *object, int index, char **target)
{
    if (object == NULL || !cJSON_IsArray(object))
    {
        *target = NULL;
        return;
    }

    cJSON *field = cJSON_GetArrayItem(object, index);
    fetch_string_common(object, field, target);
}
static inline int get_int_from_object(cJSON *object, const char *field_name)
{
    if (object == NULL || !cJSON_IsObject(object))
    {
        return INT_MIN;
    }

    cJSON *field = cJSON_GetObjectItem(object, field_name);
    if (field == NULL || !cJSON_IsNumber(field))
    {
        return INT_MIN;
    }
    return field->valueint;
}

static inline int get_int_from_array(cJSON *object, int index)
{
    if (object == NULL || !cJSON_IsArray(object))
    {
        return INT_MIN;
    }

    cJSON *field = cJSON_GetArrayItem(object, index);
    if (field == NULL || !cJSON_IsNumber(field))
    {
        return INT_MIN;
    }
    return field->valueint;
}

void sscma_client_reply_clear(sscma_client_reply_t *reply)
{
    if (reply->payload)
    {
        cJSON_Delete(reply->payload);
        reply->payload = NULL;
    }
    if (reply->data)
    {
        free(reply->data);
        reply->data = NULL;
    }
    reply->len = 0;
}

static void sscma_client_monitor(void *arg)
{
    sscma_client_handle_t client = (sscma_client_handle_t)arg;
    sscma_client_reply_t reply;
    while (true)
    {
        xQueueReceive(client->reply_queue, &reply, portMAX_DELAY);
        cJSON *type = cJSON_GetObjectItem(reply.payload, "type");
        if (type->valueint == CMD_TYPE_EVENT)
        {
            if (client->on_event)
            {
                client->on_event(client, &reply, client->user_ctx);
            }
        }
        else
        {
            if (client->on_log)
            {
                client->on_log(client, &reply, client->user_ctx);
            }
        }
        sscma_client_reply_clear(&reply);
    }
}

static void sscma_client_process(void *arg)
{
    size_t rlen = 0;
    char *suffix = NULL;
    char *prefix = NULL;
    sscma_client_handle_t client = (sscma_client_handle_t)arg;
    sscma_client_reply_t reply;
    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (client->inited == false)
        {
            continue;
        }
        if (sscma_client_available(client, &rlen) == ESP_OK && rlen)
        {
            if (rlen + client->rx_buffer.pos > client->rx_buffer.len)
            {
                rlen = client->rx_buffer.len - client->rx_buffer.pos;
                if (rlen <= 0)
                {
                    ESP_LOGW(TAG, "rx buffer is full");
                    client->rx_buffer.pos = 0;
                    continue;
                }
            }
            sscma_client_read(client, client->rx_buffer.data + client->rx_buffer.pos, rlen);
            client->rx_buffer.pos += rlen;
            client->rx_buffer.data[client->rx_buffer.pos] = 0;

            while ((suffix = strnstr(client->rx_buffer.data, RESPONSE_SUFFIX, client->rx_buffer.pos)) != NULL)
            {
                if ((prefix = strnstr(client->rx_buffer.data, RESPONSE_PREFIX, suffix - client->rx_buffer.data)) != NULL)
                {
                    int len = suffix - prefix + RESPONSE_SUFFIX_LEN;
                    reply.data = (char *)malloc(len);
                    if (reply.data != NULL)
                    {
                        reply.len = len - 1;
                        memcpy(reply.data, prefix + 1, len - 1); // remove "\r" and "\n"
                        reply.data[len - 1] = 0;
                        reply.payload = cJSON_Parse(reply.data);
                        if (reply.payload != NULL)
                        {
                            cJSON *type = cJSON_GetObjectItem(reply.payload, "type");
                            cJSON *name = cJSON_GetObjectItem(reply.payload, "name");

                            if (type == NULL || name == NULL)
                            {
                                ESP_LOGW(TAG, "invalid reply: %s", reply.data);
                                sscma_client_reply_clear(&reply);
                                continue;
                            }
                            if (type->valueint == CMD_TYPE_RESPONSE)
                            {
                                sscma_client_request_t *first_req, *next_req = NULL;
                                bool found = false;
                                if (listCURRENT_LIST_LENGTH(client->request_list) > (UBaseType_t)0)
                                {
                                    listGET_OWNER_OF_NEXT_ENTRY(first_req, client->request_list);
                                    do
                                    {
                                        listGET_OWNER_OF_NEXT_ENTRY(next_req, client->request_list);
                                        if (strncmp(next_req->cmd, name->valuestring, sizeof(next_req->cmd)) == 0)
                                        {
                                            if (next_req->reply)
                                            {
                                                found = true;
                                                if (xQueueSend(next_req->reply, &reply, 0) != pdTRUE)
                                                {
                                                    sscma_client_reply_clear(&reply); // discard this reply
                                                }
                                                break;
                                            }
                                        }
                                    } while (next_req != first_req);
                                }
                                if (!found)
                                {
                                    ESP_LOGW(TAG, "request not found: %s", name->valuestring);
                                    sscma_client_reply_clear(&reply);
                                }
                            }
                            else if (type->valueint == CMD_TYPE_LOG)
                            {
                                cJSON *code = cJSON_GetObjectItem(reply.payload, "code");
                                if (code == NULL)
                                {
                                    ESP_LOGW(TAG, "invalid log: %s", reply.data);
                                    sscma_client_reply_clear(&reply);
                                    continue;
                                }
                                if (code->valueint == CMD_EINVAL)
                                { // unkown command
                                    cJSON *data = cJSON_GetObjectItem(reply.payload, "data");
                                    if (data == NULL)
                                    {
                                        ESP_LOGW(TAG, "invalid log: %s", reply.data);
                                        sscma_client_reply_clear(&reply);
                                        continue;
                                    }
                                    sscma_client_request_t *first_req, *next_req = NULL;
                                    bool found = false;
                                    if (listCURRENT_LIST_LENGTH(client->request_list) > (UBaseType_t)0)
                                    {
                                        listGET_OWNER_OF_NEXT_ENTRY(first_req, client->request_list);
                                        do
                                        {
                                            listGET_OWNER_OF_NEXT_ENTRY(next_req, client->request_list);
                                            if (strnstr(data->valuestring, next_req->cmd, strlen(data->valuestring)) != NULL)
                                            {
                                                if (next_req->reply)
                                                {
                                                    found = true;
                                                    if (xQueueSend(next_req->reply, &reply, 0) != pdTRUE)
                                                    {
                                                        sscma_client_reply_clear(&reply); // discard this reply
                                                    }
                                                    break;
                                                }
                                            }
                                        } while (next_req != first_req);
                                    }
                                    if (!found)
                                    {
                                        ESP_LOGW(TAG, "request not found: %s", name->valuestring);
                                        sscma_client_reply_clear(&reply);
                                    }
                                }
                                else
                                {
                                    if (client->on_log == NULL || xQueueSend(client->reply_queue, &reply, 0) != pdTRUE)
                                    {
                                        sscma_client_reply_clear(&reply); // discard this reply
                                    }
                                }
                            }
                            else if (type->valueint == CMD_TYPE_EVENT)
                            {

                                sscma_client_request_t *first_req, *next_req = NULL;
                                bool found = false;
                                // discard all the events while AT+BREAK is found
                                if (listCURRENT_LIST_LENGTH(client->request_list) > (UBaseType_t)0)
                                {
                                    listGET_OWNER_OF_NEXT_ENTRY(first_req, client->request_list);
                                    do
                                    {
                                        listGET_OWNER_OF_NEXT_ENTRY(next_req, client->request_list);
                                        if (strnstr(next_req->cmd, CMD_AT_BREAK, strlen(next_req->cmd)) != NULL)
                                        {
                                            found = true;
                                            break;
                                        }
                                    } while (next_req != first_req);
                                }
                                if (client->on_event == NULL || found || xQueueSend(client->reply_queue, &reply, 0) != pdTRUE)
                                {
                                    sscma_client_reply_clear(&reply); // discard this reply
                                }
                            }
                            else
                            {
                                ESP_LOGW(TAG, "invalid reply: %s", reply.data);
                                sscma_client_reply_clear(&reply);
                            }
                        }
                    }
                    // delete this reply from rx buffer
                    memmove(client->rx_buffer.data, suffix + RESPONSE_SUFFIX_LEN, client->rx_buffer.pos - (suffix - client->rx_buffer.data) - RESPONSE_PREFIX_LEN);
                    client->rx_buffer.pos -= len;
                }
                else
                {
                    // discard this reply
                    memmove(client->rx_buffer.data, suffix + RESPONSE_SUFFIX_LEN, client->rx_buffer.pos - (suffix - client->rx_buffer.data) - RESPONSE_PREFIX_LEN);
                    client->rx_buffer.pos -= suffix - client->rx_buffer.data + RESPONSE_SUFFIX_LEN;
                    client->rx_buffer.data[client->rx_buffer.pos] = 0;
                }
            }
        }
    }
}

esp_err_t sscma_client_new(const sscma_client_io_handle_t io, const sscma_client_config_t *config, sscma_client_handle_t *ret_client)
{
#if CONFIG_SSCMA_ENABLE_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    esp_err_t ret = ESP_OK;
    sscma_client_handle_t client = NULL;
    ESP_GOTO_ON_FALSE(io && config && ret_client, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    client = (sscma_client_handle_t)malloc(sizeof(struct sscma_client_t));
    ESP_GOTO_ON_FALSE(client, ESP_ERR_NO_MEM, err, TAG, "no mem for sscma client");
    client->io = io;
    client->inited = false;

    if (config->reset_gpio_num >= 0)
    {
        if (config->flags.reset_use_expander)
        {
            client->io_expander = config->io_expander;
            ESP_GOTO_ON_FALSE(client->io_expander, ESP_ERR_INVALID_ARG, err, TAG, "invalid io expander");
            ESP_GOTO_ON_ERROR(esp_io_expander_set_dir(client->io_expander, config->reset_gpio_num, IO_EXPANDER_OUTPUT), err, TAG, "set GPIO direction failed");
        }
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = 1ULL << config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    client->rx_buffer.data = (char *)malloc(config->rx_buffer_size);
    ESP_GOTO_ON_FALSE(client->rx_buffer.data, ESP_ERR_NO_MEM, err, TAG, "no mem for rx buffer");
    client->rx_buffer.pos = 0;
    client->rx_buffer.len = config->rx_buffer_size;

    client->tx_buffer.data = (char *)malloc(config->tx_buffer_size);
    ESP_GOTO_ON_FALSE(client->tx_buffer.data, ESP_ERR_NO_MEM, err, TAG, "no mem for tx buffer");
    client->tx_buffer.pos = 0;
    client->tx_buffer.len = config->tx_buffer_size;

    client->reset_gpio_num = config->reset_gpio_num;
    client->reset_level = config->flags.reset_active_high;

    client->user_ctx = config->user_ctx;

    client->request_list = (List_t *)malloc(sizeof(List_t));
    ESP_GOTO_ON_FALSE(client->request_list, ESP_ERR_NO_MEM, err, TAG, "no mem for request list");

    client->reply_queue = xQueueCreate(config->event_queue_size, sizeof(sscma_client_reply_t));
    ESP_GOTO_ON_FALSE(client->reply_queue, ESP_ERR_NO_MEM, err, TAG, "no mem for reply queue");

    vListInitialise(client->request_list);

    BaseType_t res;

    if (config->process_task_affinity < 0)
    {
        res = xTaskCreate(sscma_client_process, "sscma_client_process", config->process_task_stack, client, config->process_task_priority, &client->process_task);
    }
    else
    {
        res = xTaskCreatePinnedToCore(sscma_client_process, "sscma_client_process", config->process_task_stack, client, config->process_task_priority, &client->process_task, config->process_task_affinity);
    }
    ESP_GOTO_ON_FALSE(res == pdPASS, ESP_FAIL, err, TAG, "create process task failed");

    if (config->monitor_task_affinity < 0)
    {
        res = xTaskCreate(sscma_client_monitor, "sscma_client_monitor", config->monitor_task_stack, client, config->monitor_task_priority, &client->monitor_task);
    }
    else
    {
        res = xTaskCreatePinnedToCore(sscma_client_monitor, "sscma_client_monitor", config->monitor_task_stack, client, config->monitor_task_priority, &client->monitor_task, config->monitor_task_affinity);
    }
    ESP_GOTO_ON_FALSE(res == pdPASS, ESP_FAIL, err, TAG, "create monitor task failed");

    client->on_event = NULL;
    client->on_log = NULL;
    *ret_client = client;

    ESP_LOGD(TAG, "new sscma client @%p", client);

    return ESP_OK;

err:
    if (client)
    {
        if (client->rx_buffer.data)
        {
            free(client->rx_buffer.data);
        }
        if (client->tx_buffer.data)
        {
            free(client->tx_buffer.data);
        }
        if (config->reset_gpio_num >= 0)
        {
            if (!config->flags.reset_use_expander)
            {
                gpio_reset_pin(client->reset_gpio_num);
            }
        }
        if (client->reply_queue)
        {
            vQueueDelete(client->reply_queue);
        }
        if (client->request_list)
        {
            free(client->request_list);
        }
        if (client->process_task)
        {
            vTaskDelete(client->process_task);
        }
        if (client->monitor_task)
        {
            vTaskDelete(client->monitor_task);
        }
        free(client);
    }
    return ret;
}

esp_err_t sscma_client_del(sscma_client_handle_t client)
{
    if (client)
    {
        if (client->reset_gpio_num >= 0)
        {
            if (client->io_expander)
            {
                esp_io_expander_set_dir(client->io_expander, client->reset_gpio_num, IO_EXPANDER_OUTPUT);
                esp_io_expander_set_level(client->io_expander, client->reset_gpio_num, client->reset_level);
                esp_io_expander_set_dir(client->io_expander, client->reset_gpio_num, IO_EXPANDER_INPUT);
            }
            else
            {
                gpio_config_t io_conf = {
                    .mode = GPIO_MODE_OUTPUT,
                    .pin_bit_mask = 1ULL << client->reset_gpio_num,
                };
                gpio_config(&io_conf);
                gpio_set_level(client->reset_gpio_num, client->reset_level);
                gpio_reset_pin(client->reset_gpio_num);
            }
        }
        vQueueDelete(client->reply_queue);

        sscma_client_request_t *first_req, *next_req = NULL;
        if (listCURRENT_LIST_LENGTH(client->request_list) > (UBaseType_t)0)
        {
            listGET_OWNER_OF_NEXT_ENTRY(first_req, client->request_list);
            do
            {
                listGET_OWNER_OF_NEXT_ENTRY(next_req, client->request_list);
                uxListRemove(&(next_req->item));
                vQueueDelete(next_req->reply);
                free(next_req);
            } while (next_req != first_req);
        }

        free(client->request_list);
        free(client->rx_buffer.data);
        free(client->tx_buffer.data);
        vTaskDelete(client->process_task);
        vTaskDelete(client->monitor_task);

        if (client->info.id != NULL)
        {
            free(client->info.id);
        }

        if (client->info.name != NULL)
        {
            free(client->info.name);
        }

        if (client->info.hw_ver != NULL)
        {
            free(client->info.hw_ver);
        }

        if (client->info.sw_ver != NULL)
        {
            free(client->info.sw_ver);
        }

        if (client->info.fw_ver != NULL)
        {
            free(client->info.fw_ver);
        }

        if (client->model.uuid != NULL)
        {
            free(client->model.uuid);
        }

        if (client->model.name != NULL)
        {
            free(client->model.name);
        }
        if (client->model.category != NULL)
        {
            free(client->model.category);
        }

        if (client->model.manufacturer != NULL)
        {
            free(client->model.manufacturer);
        }

        if (client->model.description != NULL)
        {
            free(client->model.description);
        }

        if (client->model.url != NULL)
        {
            free(client->model.url);
        }

        if (client->model.algorithm != NULL)
        {
            free(client->model.algorithm);
        }

        if (client->model.token != NULL)
        {
            free(client->model.token);
        }
        for (int i = 0; i < sizeof(client->model.classes) / sizeof(client->model.classes[0]); i++)
        {
            if (client->model.classes[i] != NULL)
            {
                free(client->model.classes[i]);
            }
        }

        free(client);
    }
    return ESP_OK;
}

esp_err_t sscma_client_init(sscma_client_handle_t client)
{
    if (!client->inited)
    {
        sscma_client_reset(client);
        client->inited = true;
    }

    memset(&client->info, 0, sizeof(sscma_client_info_t));
    memset(&client->model, 0, sizeof(sscma_client_model_t));

    return ESP_OK;
}

esp_err_t sscma_client_reset(sscma_client_handle_t client)
{
    esp_err_t ret = ESP_OK;
    vTaskSuspend(client->process_task);
    // perform hardware reset
    if (client->reset_gpio_num >= 0)
    {
        if (client->io_expander)
        {
            esp_io_expander_set_level(client->io_expander, client->reset_gpio_num, client->reset_level);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            esp_io_expander_set_level(client->io_expander, client->reset_gpio_num, !client->reset_level);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            sscma_client_request(client, CMD_PREFIX CMD_AT_BREAK CMD_SUFFIX, NULL, false, 0);
        }
        else
        {
            gpio_config_t io_conf = {
                .mode = GPIO_MODE_OUTPUT,
                .pin_bit_mask = 1ULL << client->reset_gpio_num,
            };
            gpio_config(&io_conf);
            gpio_set_level(client->reset_gpio_num, client->reset_level);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(client->reset_gpio_num, !client->reset_level);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            gpio_reset_pin(client->reset_gpio_num);
            sscma_client_request(client, CMD_PREFIX CMD_AT_BREAK CMD_SUFFIX, NULL, false, 0);
        }
    }
    else
    {
        // ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_RESET CMD_SUFFIX, NULL, false, 0), TAG, "request reset failed");
        vTaskDelay(500 / portTICK_PERIOD_MS); // wait for sscma to be ready
    }
    ESP_LOGW(TAG, "reset done");
    vTaskResume(client->process_task);
    return ret;
}

esp_err_t sscma_client_read(sscma_client_handle_t client, void *data, size_t size)
{
    return sscma_client_io_read(client->io, data, size);
}

esp_err_t sscma_client_write(sscma_client_handle_t client, const void *data, size_t size)
{
    return sscma_client_io_write(client->io, data, size);
}

esp_err_t sscma_client_available(sscma_client_handle_t client, size_t *ret_avail)
{
    return sscma_client_io_available(client->io, ret_avail);
}

esp_err_t sscma_client_register_callback(sscma_client_handle_t client, const sscma_client_callback_t *callback, void *user_ctx)
{
    if (client->on_event != NULL)
    {
        ESP_LOGW(TAG, "callback on_event already registered, overriding it");
    }
    if (client->on_log != NULL)
    {
        ESP_LOGW(TAG, "callback on_log already registered, overriding it");
    }

    client->on_event = callback->on_event;
    client->on_log = callback->on_log;
    client->user_ctx = user_ctx;

    return ESP_OK;
}

esp_err_t sscma_client_request(sscma_client_handle_t client, const char *cmd, sscma_client_reply_t *reply, bool wait, TickType_t timeout)
{
    esp_err_t ret = ESP_OK;
    sscma_client_request_t *request = NULL;

    if (wait)
    {
        request = (sscma_client_request_t *)malloc(sizeof(sscma_client_request_t));
        ESP_GOTO_ON_FALSE(request, ESP_ERR_NO_MEM, err, TAG, "no mem for request");
        request->reply = xQueueCreate(1, sizeof(sscma_client_reply_t));
        strncpy(request->cmd, &cmd[CMD_PREFIX_LEN], sizeof(request->cmd) - 1);
        request->cmd[sizeof(request->cmd) - 1] = '\0';
        for (int i = 0; i < sizeof(request->cmd); i++)
        {
            if (request->cmd[i] == '\n' || request->cmd[i] == '\r' || request->cmd[i] == '=')
            {
                request->cmd[i] = '\0';
            }
        }
        ESP_GOTO_ON_FALSE(request->reply, ESP_ERR_NO_MEM, err, TAG, "no mem for reply");
        vListInitialiseItem(&(request->item));
        listSET_LIST_ITEM_OWNER(&(request->item), request);
        vListInsertEnd(client->request_list, &(request->item));
    }

    ESP_GOTO_ON_ERROR(sscma_client_write(client, cmd, strlen(cmd)), err, TAG, "write command failed");

    if (wait)
    {
        if (xQueueReceive(request->reply, reply, timeout) == pdTRUE)
        {
            ret = ESP_OK;
        }
        else
        {
            ret = ESP_ERR_TIMEOUT;
        }
    }

err:
    if (wait)
    {
        if (listIS_CONTAINED_WITHIN(client->request_list, &(request->item)))
        {
            uxListRemove(&(request->item));
        }
        if (request)
        {
            vQueueDelete(request->reply);
            free(request);
        }
    }
    return ret;
}

esp_err_t sscma_client_get_info(sscma_client_handle_t client, sscma_client_info_t **info, bool cached)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;

    *info = &client->info;

    if (cached && client->info.id != NULL)
    {
        return ret;
    }

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_ID CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request id failed");

    if (reply.payload != NULL)
    {
        fetch_string_from_object(reply.payload, "data", &client->info.id);

        sscma_client_reply_clear(&reply);
    }

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_NAME CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request name failed");

    if (reply.payload != NULL)
    {
        fetch_string_from_object(reply.payload, "data", &(client->info.name));

        sscma_client_reply_clear(&reply);
    }

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_VERSION CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request version failed");

    if (reply.payload != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(reply.payload, "data");
        if (data != NULL)
        {
            fetch_string_from_object(data, "hardware", &(client->info.hw_ver));
            fetch_string_from_object(data, "software", &(client->info.fw_ver));
            fetch_string_from_object(data, "at_api", &(client->info.sw_ver));
        }

        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_get_model(sscma_client_handle_t client, sscma_client_model_t **model, bool cached)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;
    bool is_changed = false;
    char *model_data = NULL;
    size_t len = 0;
    *model = &client->model;

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_MODEL CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request model failed");

    if (reply.payload != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(reply.payload, "data");
        if (data != NULL)
        {
            if (client->model.id != get_int_from_object(data, "id"))
            {
                is_changed = true;
                client->model.id = get_int_from_object(data, "id");
            }
        }
        sscma_client_reply_clear(&reply);
    }

    if (cached && !is_changed && client->model.uuid != NULL)
    {
        return ret;
    }

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_INFO CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request model failed");

    if (reply.payload != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(reply.payload, "data");
        if (data != NULL)
        {
            cJSON *info = cJSON_GetObjectItem(data, "info");
            if (info != NULL && cJSON_IsString(info))
            {
                model_data = malloc(strlen(info->valuestring));
                if (model_data != NULL)
                {
                    if (mbedtls_base64_decode((unsigned char *)model_data, strlen(info->valuestring), &len, (unsigned char *)info->valuestring, strlen(info->valuestring)) == 0)
                    {
                        cJSON *root = cJSON_Parse(model_data);
                        if (root != NULL)
                        {
                            fetch_string_from_object(root, "uuid", &client->model.uuid);
                            fetch_string_from_object(root, "name", &client->model.name);
                            fetch_string_from_object(root, "version", &client->model.ver);
                            fetch_string_from_object(root, "category", &client->model.category);
                            fetch_string_from_object(root, "algorithm", &client->model.algorithm);
                            fetch_string_from_object(root, "url", &client->model.url);
                            fetch_string_from_object(root, "key", &client->model.token);
                            fetch_string_from_object(root, "author", &client->model.manufacturer);
                            fetch_string_from_object(root, "description", &client->model.description);
                            cJSON *classes = cJSON_GetObjectItem(root, "classes");
                            if (classes != NULL && cJSON_IsArray(classes))
                            {
                                for (int i = 0; i < cJSON_GetArraySize(classes); i++)
                                {
                                    fetch_string_from_array(classes, i, &client->model.classes[i]);
                                }
                            }
                        }
                        cJSON_Delete(root);
                    }
                }
                free(model_data);
            }
        }
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_set_model(sscma_client_handle_t client, int model)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;
    int code = 0;
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), CMD_PREFIX CMD_AT_MODEL CMD_SET "%d" CMD_SUFFIX, model);

    ESP_RETURN_ON_ERROR(sscma_client_request(client, cmd, &reply, true, CMD_WAIT_DELAY), TAG, "request model failed");

    if (reply.payload != NULL)
    {
        code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_sample(sscma_client_handle_t client, int times)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;
    int code = 0;
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), CMD_PREFIX CMD_AT_SAMPLE CMD_SET "%d" CMD_SUFFIX, times);
    ESP_RETURN_ON_ERROR(sscma_client_request(client, cmd, &reply, true, CMD_WAIT_DELAY), TAG, "request sample failed");

    if (reply.payload != NULL)
    {
        code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_invoke(sscma_client_handle_t client, int times, bool fliter, bool show)
{
    esp_err_t ret = ESP_OK;
    char cmd[64] = {0};
    int code = 0;
    sscma_client_reply_t reply;

    snprintf(cmd, sizeof(cmd), CMD_PREFIX CMD_AT_INVOKE CMD_SET "%d,%d,%d" CMD_SUFFIX, times, fliter ? 1 : 0, show ? 0 : 1);

    ESP_RETURN_ON_ERROR(sscma_client_request(client, cmd, &reply, true, CMD_WAIT_DELAY), TAG, "request invoke failed");

    if (reply.payload != NULL)
    {
        code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_set_sensor(sscma_client_handle_t client, int id, int opt_id, bool enable)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;
    char cmd[64] = {0};

    snprintf(cmd, sizeof(cmd), CMD_PREFIX CMD_AT_SENSOR CMD_SET "%d,%d,%d" CMD_SUFFIX, id, enable ? 1 : 0, opt_id);

    ESP_RETURN_ON_ERROR(sscma_client_request(client, cmd, &reply, true, CMD_WAIT_DELAY), TAG, "request set sensor failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_get_sensor(sscma_client_handle_t client, sscma_client_sensor_t *sensor)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_SENSOR CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request get sensor failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        if (ret == ESP_OK)
        {
            cJSON *data = cJSON_GetObjectItem(reply.payload, "data");
            if (data != NULL)
            {
                cJSON *o_sensor = cJSON_GetObjectItem(data, "sensor");
                if (sensor != NULL)
                {
                    sensor->id = get_int_from_object(o_sensor, "id");
                    sensor->type = get_int_from_object(o_sensor, "type");
                    sensor->state = get_int_from_object(o_sensor, "state");
                    sensor->opt_id = get_int_from_object(o_sensor, "opt_id");
                    fetch_string_from_object(o_sensor, "opt_detail", &(sensor->opt_detail));
                }
            }
        }
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_break(sscma_client_handle_t client)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_BREAK CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request break failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_set_iou_threshold(sscma_client_handle_t client, int threshold)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;
    char cmd[64] = {0};

    snprintf(cmd, sizeof(cmd), CMD_PREFIX CMD_AT_TIOU CMD_SET "%d" CMD_SUFFIX, threshold);

    ESP_RETURN_ON_ERROR(sscma_client_request(client, cmd, &reply, true, CMD_WAIT_DELAY), TAG, "request set iou failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_get_iou_threshold(sscma_client_handle_t client, int *threshold)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;

    ESP_RETURN_ON_FALSE(threshold != NULL, ESP_ERR_INVALID_ARG, TAG, "threshold is NULL");

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_TIOU CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request get iou failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        if (ret == ESP_OK)
        {

            *threshold = get_int_from_object(reply.payload, "data");
        }
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_set_confidence_threshold(sscma_client_handle_t client, int threshold)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;
    char cmd[64] = {0};

    snprintf(cmd, sizeof(cmd), CMD_PREFIX CMD_AT_TSCORE CMD_SET "%d" CMD_SUFFIX, threshold);

    ESP_RETURN_ON_ERROR(sscma_client_request(client, cmd, &reply, true, CMD_WAIT_DELAY), TAG, "request set confidence failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_client_get_confidence_threshold(sscma_client_handle_t client, int *threshold)
{
    esp_err_t ret = ESP_OK;
    sscma_client_reply_t reply;

    ESP_RETURN_ON_FALSE(threshold != NULL, ESP_ERR_INVALID_ARG, TAG, "threshold is NULL");

    ESP_RETURN_ON_ERROR(sscma_client_request(client, CMD_PREFIX CMD_AT_TSCORE CMD_QUERY CMD_SUFFIX, &reply, true, CMD_WAIT_DELAY), TAG, "request get confidence failed");

    if (reply.payload != NULL)
    {
        int code = get_int_from_object(reply.payload, "code");
        ret = SSCMA_CLIENT_CMD_ERROR_CODE(code);
        if (ret == ESP_OK)
        {

            *threshold = get_int_from_object(reply.payload, "data");
        }
        sscma_client_reply_clear(&reply);
    }

    return ret;
}

esp_err_t sscma_utils_fetch_boxes_from_reply(sscma_client_reply_t *reply, sscma_client_box_t **boxes, int *num_boxes)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(reply != NULL, ESP_ERR_INVALID_ARG, TAG, "reply is NULL");
    ESP_RETURN_ON_FALSE(boxes != NULL, ESP_ERR_INVALID_ARG, TAG, "boxes is NULL");
    ESP_RETURN_ON_FALSE(num_boxes != NULL, ESP_ERR_INVALID_ARG, TAG, "num_boxes is NULL");
    ESP_RETURN_ON_FALSE(cJSON_IsObject(reply->payload), ESP_ERR_INVALID_ARG, TAG, "reply is not object");

    *boxes = NULL;
    *num_boxes = 0;

    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (data != NULL)
    {
        cJSON *boxes_data = cJSON_GetObjectItem(data, "boxes");
        if (boxes_data == NULL)
            return ESP_OK;
        *num_boxes = cJSON_GetArraySize(boxes_data);
        if (*num_boxes == 0)
            return ESP_OK;
        *boxes = malloc(sizeof(sscma_client_box_t) * (*num_boxes));
        ESP_RETURN_ON_FALSE(*boxes != NULL, ESP_ERR_NO_MEM, TAG, "malloc boxes failed");
        for (int i = 0; i < *num_boxes; i++)
        {
            cJSON *box = cJSON_GetArrayItem(boxes_data, i);
            if (box != NULL)
            {
                (*boxes)[i].x = get_int_from_array(box, 0);
                (*boxes)[i].y = get_int_from_array(box, 1);
                (*boxes)[i].w = get_int_from_array(box, 2);
                (*boxes)[i].h = get_int_from_array(box, 3);
                (*boxes)[i].score = get_int_from_array(box, 4);
                (*boxes)[i].target = get_int_from_array(box, 5);
            }
        }
    }

    return ret;
}

esp_err_t sscma_utils_prase_boxes_from_reply(sscma_client_reply_t *reply, sscma_client_box_t *boxes, int max_boxes, int *num_boxes)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(reply != NULL, ESP_ERR_INVALID_ARG, TAG, "reply is NULL");
    ESP_RETURN_ON_FALSE(boxes != NULL, ESP_ERR_INVALID_ARG, TAG, "classes is NULL");
    ESP_RETURN_ON_FALSE(num_boxes != NULL, ESP_ERR_INVALID_ARG, TAG, "num_classes is NULL");
    ESP_RETURN_ON_FALSE(cJSON_IsObject(reply->payload), ESP_ERR_INVALID_ARG, TAG, "reply is not object");

    *num_boxes = 0;

    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (data != NULL)
    {
        cJSON *boxes_data = cJSON_GetObjectItem(data, "boxes");
        if (boxes_data == NULL)
            return ESP_OK;
        *num_boxes = cJSON_GetArraySize(boxes_data) > max_boxes ? max_boxes : cJSON_GetArraySize(boxes_data);
        if (*num_boxes == 0)
            return ESP_OK;
        for (int i = 0; i < *num_boxes; i++)
        {
            cJSON *item = cJSON_GetArrayItem(boxes_data, i);
            if (item != NULL)
            {
                boxes[i].x = get_int_from_array(item, 0);
                boxes[i].y = get_int_from_array(item, 1);
                boxes[i].w = get_int_from_array(item, 2);
                boxes[i].h = get_int_from_array(item, 3);
                boxes[i].score = get_int_from_array(item, 4);
                boxes[i].target = get_int_from_array(item, 5);
            }
        }
    }

    return ret;
}
esp_err_t sscma_utils_fetch_classes_from_reply(sscma_client_reply_t *reply, sscma_client_class_t **classes, int *num_classes)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(reply != NULL, ESP_ERR_INVALID_ARG, TAG, "reply is NULL");
    ESP_RETURN_ON_FALSE(classes != NULL, ESP_ERR_INVALID_ARG, TAG, "classes is NULL");
    ESP_RETURN_ON_FALSE(num_classes != NULL, ESP_ERR_INVALID_ARG, TAG, "num_classes is NULL");
    ESP_RETURN_ON_FALSE(cJSON_IsObject(reply->payload), ESP_ERR_INVALID_ARG, TAG, "reply is not object");

    *classes = NULL;
    *num_classes = 0;

    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (data != NULL)
    {
        cJSON *classes_data = cJSON_GetObjectItem(data, "classes");
        if (classes_data == NULL)
            return ESP_OK;
        *num_classes = cJSON_GetArraySize(classes_data);
        if (*num_classes == 0)
            return ESP_OK;
        *classes = malloc(sizeof(sscma_client_class_t) * (*num_classes));
        ESP_RETURN_ON_FALSE(*classes != NULL, ESP_ERR_NO_MEM, TAG, "malloc classes failed");
        for (int i = 0; i < *num_classes; i++)
        {
            cJSON *item = cJSON_GetArrayItem(classes_data, i);
            if (item != NULL)
            {
                (*classes)[i].target = get_int_from_array(item, 0);
                (*classes)[i].score = get_int_from_array(item, 1);
            }
        }
    }

    return ret;
}

esp_err_t sscma_utils_prase_classes_from_reply(sscma_client_reply_t *reply, sscma_client_class_t *classes, int max_classes, int *num_classes)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(reply != NULL, ESP_ERR_INVALID_ARG, TAG, "reply is NULL");
    ESP_RETURN_ON_FALSE(classes != NULL, ESP_ERR_INVALID_ARG, TAG, "classes is NULL");
    ESP_RETURN_ON_FALSE(num_classes != NULL, ESP_ERR_INVALID_ARG, TAG, "num_classes is NULL");
    ESP_RETURN_ON_FALSE(cJSON_IsObject(reply->payload), ESP_ERR_INVALID_ARG, TAG, "reply is not object");

    *num_classes = 0;

    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (data != NULL)
    {
        cJSON *classes_data = cJSON_GetObjectItem(data, "classes");
        if (classes_data == NULL)
            return ESP_OK;
        *num_classes = cJSON_GetArraySize(classes_data) > max_classes ? max_classes : cJSON_GetArraySize(classes_data);
        if (*num_classes == 0)
            return ESP_OK;
        for (int i = 0; i < *num_classes; i++)
        {
            cJSON *item = cJSON_GetArrayItem(classes_data, i);
            if (item != NULL)
            {
                classes[i].target = get_int_from_array(item, 0);
                classes[i].score = get_int_from_array(item, 1);
            }
        }
    }

    return ret;
}

esp_err_t sscma_utils_fetch_image_from_reply(sscma_client_reply_t *reply, char **image, int *image_size)
{
    ESP_RETURN_ON_FALSE(reply && image && image_size, ESP_ERR_INVALID_ARG, TAG, "Invalid argument(s) detected");

    *image = NULL;
    *image_size = 0;

    if (!cJSON_IsObject(reply->payload))
    {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (!data)
    {
        return ESP_FAIL;
    }

    cJSON *image_data = cJSON_GetObjectItem(data, "image");
    if (!image_data)
    {
        return ESP_FAIL;
    }

    const char *image_str = cJSON_GetStringValue(image_data);
    if (!image_str)
    {
        return ESP_FAIL;
    }

    *image = strdup(image_str);
    if (!(*image))
    {
        return ESP_ERR_NO_MEM;
    }

    *image_size = strlen(image_str);

    return ESP_OK;
}

esp_err_t sscma_utils_prase_image_from_reply(sscma_client_reply_t *reply, char *image, int max_image_size, int *image_size)
{
    ESP_RETURN_ON_FALSE(reply && image && image_size, ESP_ERR_INVALID_ARG, TAG, "Invalid argument(s) detected");

    if (!cJSON_IsObject(reply->payload))
    {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (!data)
    {
        return ESP_FAIL;
    }

    cJSON *image_data = cJSON_GetObjectItem(data, "image");
    if (!image_data)
    {
        return ESP_FAIL;
    }

    const char *image_str = cJSON_GetStringValue(image_data);
    if (!image_str)
    {
        return ESP_FAIL;
    }

    size_t image_str_len = strlen(image_str);
    if (image_str_len > max_image_size)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    *image_size = image_str_len;
    strcpy(image, image_str);

    return ESP_OK;
}