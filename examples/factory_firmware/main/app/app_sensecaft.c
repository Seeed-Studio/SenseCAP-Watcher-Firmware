#include "app_sensecraft.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <mbedtls/base64.h>

#include "event_loops.h"
#include "data_defs.h"
#include "deviceinfo.h"
#include "util.h"
#include "uuid.h"

static const char *TAG = "sensecaft";

struct app_sensecraft *gp_sensecraft = NULL;

const int MQTT_PUB_QOS = 0;

static void __data_lock(struct app_sensecraft  *p_sensecraft)
{
    xSemaphoreTake(p_sensecraft->sem_handle, portMAX_DELAY);
}
static void __data_unlock(struct app_sensecraft *p_sensecraft)
{
    xSemaphoreGive(p_sensecraft->sem_handle);  
}

/*************************************************************************
 * HTTPS Request
 ************************************************************************/
static char *__request( const char *base_url, 
                        const char *api_key, 
                        const char *endpoint, 
                        const char *content_type, 
                        esp_http_client_method_t method, 
                        const char *boundary, 
                        uint8_t *data, size_t len)
{
    esp_err_t ret = ESP_OK;
    char *url = NULL;
    char *result = NULL;
    asprintf(&url, "%s%s", base_url, endpoint);
    if( url == NULL ) {
        ESP_LOGE(TAG, "Failed to allocate url");
        return NULL;
    }
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *headers = NULL;
    if (boundary)
    {
        asprintf(&headers, "%s; boundary=%s", content_type, boundary);
    }
    else
    {
        asprintf(&headers, "%s", content_type);
    }
    ESP_GOTO_ON_FALSE(headers != NULL, ESP_FAIL, end, TAG, "Failed to allocate headers!");

    esp_http_client_set_header(client, "Content-Type", headers);
    free(headers);

    if (api_key != NULL)
    {
        asprintf(&headers, "Device %s", api_key);
        ESP_GOTO_ON_FALSE(headers != NULL, ESP_FAIL, end, TAG, "Failed to allocate headers!");
        esp_http_client_set_header(client, "Authorization", headers);
        free(headers);
    }

    esp_err_t err = esp_http_client_open(client, len);
    ESP_GOTO_ON_FALSE(err == ESP_OK, ESP_FAIL, end, TAG, "Failed to open client!");
    if (len > 0)
    {
        int wlen = esp_http_client_write(client, (const char *)data, len);
        ESP_GOTO_ON_FALSE(wlen >= 0, ESP_FAIL, end, TAG, "Failed to open client!");
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (esp_http_client_is_chunked_response(client))
    {
        esp_http_client_get_chunk_length(client, &content_length);
    }
    ESP_LOGI(TAG, "content_length=%d", content_length);
    ESP_GOTO_ON_FALSE(content_length >= 0, ESP_FAIL, end, TAG, "HTTP client fetch headers failed!");

    result = (char *)psram_malloc(content_length + 1);
    int read = esp_http_client_read_response(client, result, content_length);
    if (read != content_length)
    {
        ESP_LOGE(TAG, "HTTP_ERROR: read=%d, length=%d", read, content_length);
        free(result);
        result = NULL;
    }
    else
    {
        result[content_length] = 0;
        ESP_LOGD(TAG, "content: %s, size: %d", result, strlen(result));
    }

end:
    free(url);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result != NULL ? result : NULL;
}

static int __https_mqtt_token_get(struct sensecraft_mqtt_connect_info *p_info, 
                                  const char *p_token)
{
    char *result = __request(SENSECAP_URL, p_token, SENSECAP_PATH_TOKEN_GET, "application/json", HTTP_METHOD_GET, NULL, NULL,0);
    if (result == NULL)
    {
        ESP_LOGE(TAG, "request failed");
        return -1;
    }
    cJSON *root = cJSON_Parse(result);
    if (root == NULL) {
        ESP_LOGE(TAG,"Error before: [%s]\n", cJSON_GetErrorPtr());
        return -1;
    }
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (  code->valueint != 0) {
        ESP_LOGE(TAG,"Code: %d\n", code->valueint);
        free(result);
        return -1;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data != NULL && cJSON_IsObject(data)) {

        cJSON *serverUrl_json = cJSON_GetObjectItem(data, "serverUrl");
        if (serverUrl_json != NULL && cJSON_IsString(serverUrl_json)) {
            strcpy(p_info->serverUrl, serverUrl_json->valuestring);
        }
        cJSON *token_json = cJSON_GetObjectItem(data, "token");
        if (token_json != NULL && cJSON_IsString(token_json)) {
            strcpy(p_info->token, token_json->valuestring);
        }
        cJSON *expiresIn_json = cJSON_GetObjectItem(data, "expiresIn");
        if (expiresIn_json != NULL && cJSON_IsString(expiresIn_json)) {
            p_info->expiresIn = atoll(expiresIn_json->valuestring)/1000;
        }
        cJSON *mqttPort_json = cJSON_GetObjectItem(data, "mqttPort");
        if (mqttPort_json != NULL && cJSON_IsString(mqttPort_json)) {
            p_info->mqttPort = atoi(mqttPort_json->valuestring);
        }
        cJSON *mqttsPort_json = cJSON_GetObjectItem(data, "mqttsPort");
        if (mqttsPort_json != NULL && cJSON_IsString(mqttsPort_json)) {
            p_info->mqttsPort = atoi(mqttsPort_json->valuestring);
        }
    }
    ESP_LOGI(TAG, "Server URL: %s", p_info->serverUrl);
    ESP_LOGI(TAG, "Token: %s", p_info->token);
    ESP_LOGI(TAG, "Expires In: %d", p_info->expiresIn);
    ESP_LOGI(TAG, "MQTT Port: %d", p_info->mqttPort);
    ESP_LOGI(TAG, "MQTTS Port: %d", p_info->mqttsPort);

    cJSON_Delete(root);
    free(result);
    return 0;
}

/*************************************************************************
 * MQTT Client
 ************************************************************************/
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * This func is called by __mqtt_event_handler, i.e. under the context of MQTT task 
 * (the MQTT task is within the esp-mqtt component)
 * It's OK to process a MQTT msg with max length = 2048 (2048 is inbox buffer size of
 * MQTT compoent, configured with menuconfig)
*/
static void __parse_mqtt_tasklist(char *mqtt_msg_buff, int msg_buff_len)
{
    esp_err_t  ret = ESP_OK;

    ESP_LOGI(TAG, "start to parse tasklist from MQTT msg ...");
    ESP_LOGD(TAG, "MQTT msg: \r\n %.*s\r\nlen=%d", msg_buff_len, mqtt_msg_buff, msg_buff_len);
    
    cJSON *json_root = cJSON_Parse(mqtt_msg_buff);
    if (json_root == NULL) {
        ESP_LOGE(TAG, "Failed to parse cJSON object for MQTT msg:");
        ESP_LOGE(TAG, "%.*s\r\n", msg_buff_len, mqtt_msg_buff);
        return;
    }

    cJSON *requestid = cJSON_GetObjectItem(json_root, "requestId");
    if ( requestid == NULL || !cJSON_IsString(requestid)) {
        ESP_LOGE(TAG, "requestid is not a string\n");
        cJSON_Delete(json_root);
        return;
    }

    cJSON *order_arr = cJSON_GetObjectItem(json_root, "order");
    if ( order_arr == NULL || !cJSON_IsArray(order_arr)) {
        ESP_LOGE(TAG, "Order field is not an array\n");
        cJSON_Delete(json_root);
        return;
    }

    cJSON *order_arr_0 = cJSON_GetArrayItem(order_arr, 0);
    if( order_arr_0 == NULL || !cJSON_IsObject(order_arr_0)) {
        cJSON_Delete(json_root);
        return;
    }
    cJSON *value = cJSON_GetObjectItem(order_arr_0, "value");
    if( value == NULL || !cJSON_IsObject(value) ) {
        cJSON_Delete(json_root);
        return;
    }
    cJSON *tl = cJSON_GetObjectItem(value, "tl");
    if( tl == NULL || !cJSON_IsObject(tl) ) {
        cJSON_Delete(json_root);
        return;
    }

    char *tl_str = cJSON_PrintUnformatted(tl);

    ret = app_sensecraft_mqtt_report_taskflow_ack( requestid->valuestring, tl_str, strlen(tl_str));
    if( ret != ESP_OK ) {
        ESP_LOGW(TAG, "Failed to report taskflow ack to MQTT server");
    }

    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_TASK_FLOW_START_BY_MQTT, 
                                &tl_str,
                                sizeof(void *), /* ptr size */
                                portMAX_DELAY);    

    cJSON_Delete(json_root);
}

static void __parse_mqtt_version_notify(char *mqtt_msg_buff, int msg_buff_len)
{
    ESP_LOGI(TAG, "start to parse version-notify from MQTT msg ...");
    ESP_LOGD(TAG, "MQTT msg: \r\n %.*s\r\nlen=%d", msg_buff_len, mqtt_msg_buff, msg_buff_len);
    
    cJSON *tmp_cjson = cJSON_Parse(mqtt_msg_buff);

    if (tmp_cjson == NULL) {
        ESP_LOGE(TAG, "failed to parse cJSON object for MQTT msg:");
        ESP_LOGE(TAG, "%.*s\r\n", msg_buff_len, mqtt_msg_buff);
        return;
    }
    // since there's only one consumer of version-notify msg, we just post it to event loop,
    // it's up to the consumer to free the memory of the cJSON object.
    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_MQTT_OTA_JSON,
                                    &tmp_cjson,
                                    sizeof(void *), /* ptr size */
                                    portMAX_DELAY);
}

static void __mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    struct app_sensecraft *p_sensecraft = (struct app_sensecraft *)handler_args;
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            p_sensecraft->mqtt_connected_flag = true;
            esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_MQTT_CONNECTED, 
                                NULL, 0, portMAX_DELAY);

            // TODO maybe repeat subscribe ?
            msg_id = esp_mqtt_client_subscribe(client, p_sensecraft->topic_down_task_publish, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d, topic=%s", msg_id, p_sensecraft->topic_down_task_publish);

            msg_id = esp_mqtt_client_subscribe(client, p_sensecraft->topic_down_version_notify, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d, topic=%s", msg_id, p_sensecraft->topic_down_version_notify);

            if( p_sensecraft->p_mqtt_recv_buf ) {
                free(p_sensecraft->p_mqtt_recv_buf);
                p_sensecraft->p_mqtt_recv_buf = NULL;
            }

            break;
        case MQTT_EVENT_DISCONNECTED:
            if( p_sensecraft->p_mqtt_recv_buf ) {
                free(p_sensecraft->p_mqtt_recv_buf);
                p_sensecraft->p_mqtt_recv_buf = NULL;
            } 
            p_sensecraft->mqtt_connected_flag = false;
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:

            if( event->total_data_len !=  event->data_len ) {
                if( event->current_data_offset == 0 ) {
                    ESP_LOGI(TAG, "START RECV:%d", event->total_data_len);
                    memset(p_sensecraft->topic_cache, 0, sizeof(p_sensecraft->topic_cache));
                    memcpy(p_sensecraft->topic_cache, event->topic, event->topic_len);
                    if( p_sensecraft->p_mqtt_recv_buf ) {
                        free(p_sensecraft->p_mqtt_recv_buf);
                        p_sensecraft->p_mqtt_recv_buf = NULL;
                    }
                    p_sensecraft->p_mqtt_recv_buf = psram_malloc(event->total_data_len);
                    if( p_sensecraft->p_mqtt_recv_buf == NULL ) {
                        ESP_LOGE(TAG, "psram_malloc %d failed", event->total_data_len);
                        break;
                    }
                }

                if( p_sensecraft->p_mqtt_recv_buf != NULL ) {
                    memcpy( p_sensecraft->p_mqtt_recv_buf + event->current_data_offset, event->data, event->data_len);  
                }

                if( (event->current_data_offset + event->data_len) != event->total_data_len ) {
                    ESP_LOGI(TAG, "RECV DATA len:%d, offset:%d", event->data_len, event->current_data_offset);
                    break;
                }
                ESP_LOGI(TAG, "RECV END: len:%d", event->total_data_len);
            } else {
                if( p_sensecraft->p_mqtt_recv_buf ) {
                    free(p_sensecraft->p_mqtt_recv_buf);
                    p_sensecraft->p_mqtt_recv_buf = NULL;
                }
            }
            
            ESP_LOGI(TAG, "MQTT_EVENT_DATA TOPIC=%.*s", event->topic_len, event->topic);
            // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            // printf("DATA=%.*s\r\n", event->total_data_len, event->data);  
            
            char *p_data = NULL;
            char *p_topic = NULL;
            size_t len = 0;

            if( p_sensecraft->p_mqtt_recv_buf ) {
                p_data = p_sensecraft->p_mqtt_recv_buf;
                len = event->total_data_len;
            } else if( event->total_data_len ==  event->data_len){
                p_data = event->data;
                len = event->total_data_len;
                p_topic = event->topic;
                memset(p_sensecraft->topic_cache, 0, sizeof(p_sensecraft->topic_cache));
                memcpy(p_sensecraft->topic_cache, event->topic, event->topic_len);
            } else {
                ESP_LOGE(TAG, "Receive exception");
                break;
            }

            // handle data
            if (strstr(p_sensecraft->topic_cache, "task-publish")) {
                __parse_mqtt_tasklist(p_data, len);
            } else if (strstr(p_sensecraft->topic_cache, "version-notify")) {
                __parse_mqtt_version_notify(p_data, len);
            }

            if( p_sensecraft->p_mqtt_recv_buf ) {
                free(p_sensecraft->p_mqtt_recv_buf);
                p_sensecraft->p_mqtt_recv_buf = NULL;
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static void __sensecraft_task(void *p_arg)
{
    ESP_LOGI(TAG, "sensecraft start:%s", SENSECAP_URL);
    struct app_sensecraft *p_sensecraft = (struct app_sensecraft *)p_arg;
    struct sensecraft_mqtt_connect_info *p_mqtt_info = &p_sensecraft->mqtt_info;

    esp_err_t ret = 0;
    time_t now = 0;
    bool mqtt_client_inited = false;
    size_t len =0;
    bool  is_need_update_token = false;

    sniprintf(p_sensecraft->topic_down_task_publish, MQTT_TOPIC_STR_LEN, 
                "iot/ipnode/%s/get/order/task-publish", p_sensecraft->deviceinfo.eui);
    sniprintf(p_sensecraft->topic_down_version_notify, MQTT_TOPIC_STR_LEN, 
                "iot/ipnode/%s/get/order/version-notify", p_sensecraft->deviceinfo.eui);
    sniprintf(p_sensecraft->topic_up_task_publish_ack, MQTT_TOPIC_STR_LEN, 
                "iot/ipnode/%s/update/order/task-publish-ack", p_sensecraft->deviceinfo.eui);
    sniprintf(p_sensecraft->topic_up_change_device_status, MQTT_TOPIC_STR_LEN, 
                "iot/ipnode/%s/update/event/change-device-status", p_sensecraft->deviceinfo.eui);
    sniprintf(p_sensecraft->topic_up_warn_event_report, MQTT_TOPIC_STR_LEN, 
                "iot/ipnode/%s/update/event/measure-sensor", p_sensecraft->deviceinfo.eui);

    while (1) {
        
        xSemaphoreTake(p_sensecraft->net_sem_handle, pdMS_TO_TICKS(10000));
        if (!p_sensecraft->net_flag ) {
            continue;
        }

        time(&now);  // now is seconds since unix epoch
        if( p_sensecraft->timesync_flag ) {
            if ((p_mqtt_info->expiresIn) < ((int)now + 60))  {
                is_need_update_token = true;
            } 
        } else {
            if( p_sensecraft->last_http_time == 0 ) {
                p_sensecraft->last_http_time = now;
                is_need_update_token = true;
            } else if( difftime(now, p_sensecraft->last_http_time) > (60*60) ) {
                p_sensecraft->last_http_time = now;
                is_need_update_token = true;
            }
        }

        if( is_need_update_token ) {
            is_need_update_token = false;
            ESP_LOGI(TAG, "mqtt token is near expiration, now: %d, expire: %d, refresh it ...", (int)now, (p_mqtt_info->expiresIn));
            
            ret = __https_mqtt_token_get(p_mqtt_info, (const char *)p_sensecraft->https_token);
            if( ret == 0 ) {

                snprintf(p_sensecraft->mqtt_broker_uri, sizeof(p_sensecraft->mqtt_broker_uri), "mqtt://%s:%d", \
                                                p_mqtt_info->serverUrl, p_mqtt_info->mqttPort);
                snprintf(p_sensecraft->mqtt_client_id,  sizeof(p_sensecraft->mqtt_client_id), "device-3000-%s", p_sensecraft->deviceinfo.eui);
                memcpy(p_sensecraft->mqtt_password, p_mqtt_info->token, sizeof(p_mqtt_info->token));

                ESP_LOGI(TAG, "mqtt connect info changed, uri: %s", p_sensecraft->mqtt_broker_uri);
                p_sensecraft->mqtt_cfg.broker.address.uri = p_sensecraft->mqtt_broker_uri;
                p_sensecraft->mqtt_cfg.credentials.username = p_sensecraft->mqtt_client_id;
                p_sensecraft->mqtt_cfg.credentials.client_id = p_sensecraft->mqtt_client_id;
                p_sensecraft->mqtt_cfg.credentials.authentication.password = p_sensecraft->mqtt_password;
                p_sensecraft->mqtt_cfg.session.disable_clean_session = true;
                p_sensecraft->mqtt_cfg.network.disable_auto_reconnect = false;
                p_sensecraft->mqtt_cfg.network.reconnect_timeout_ms = 15000; // undetermined

                if (!mqtt_client_inited) {
                    p_sensecraft->mqtt_handle = esp_mqtt_client_init(&p_sensecraft->mqtt_cfg);
                    esp_mqtt_client_register_event(p_sensecraft->mqtt_handle, ESP_EVENT_ANY_ID, __mqtt_event_handler, p_sensecraft);
                    esp_mqtt_client_start(p_sensecraft->mqtt_handle);
                    mqtt_client_inited = true;
                    ESP_LOGI(TAG, "mqtt client started!");
                } else {
                    esp_mqtt_set_config(p_sensecraft->mqtt_handle, &p_sensecraft->mqtt_cfg);
                    esp_mqtt_client_reconnect(p_sensecraft->mqtt_handle);
                    ESP_LOGI(TAG, "mqtt client start reconnecting ...");
                }
            }
        }

    } 
}

static void __event_loop_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    struct app_sensecraft *p_sensecraft = (struct app_sensecraft *)handler_args;

    if ( base == CTRL_EVENT_BASE) {
        switch (id) {
            case CTRL_EVENT_SNTP_TIME_SYNCED:
            {
                ESP_LOGI(TAG, "received event: CTRL_EVENT_SNTP_TIME_SYNCED");
                p_sensecraft->timesync_flag = true;
                break;
            }
        default:
            break;
        }
    } else if( base == VIEW_EVENT_BASE) {
        switch (id) {
            case VIEW_EVENT_WIFI_ST:
            {
                ESP_LOGI(TAG, "event: VIEW_EVENT_WIFI_ST");
                struct view_data_wifi_st *p_st = (struct view_data_wifi_st *)event_data;
                if (p_st->is_network) {
                    p_sensecraft->net_flag = true;
                } else {
                    p_sensecraft->net_flag = false;
                }
                xSemaphoreGive(p_sensecraft->net_sem_handle);
                break;
            }
        default:
            break;
        }
    }
}


/*************************************************************************
 * API
 ************************************************************************/
esp_err_t app_sensecraft_init(void)
{
#if CONFIG_ENABLE_FACTORY_FW_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif

    esp_err_t ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = NULL;
    gp_sensecraft = (struct app_sensecraft *) psram_malloc(sizeof(struct app_sensecraft));
    if (gp_sensecraft == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    p_sensecraft = gp_sensecraft;
    memset(p_sensecraft, 0, sizeof( struct app_sensecraft ));
    
    ret  = deviceinfo_get(&p_sensecraft->deviceinfo);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "deviceinfo read fail %d!", ret);

    ret = app_sensecraft_https_token_gen(&p_sensecraft->deviceinfo, (char *)p_sensecraft->https_token, sizeof(p_sensecraft->https_token));
    ESP_GOTO_ON_ERROR(ret, err, TAG, "sensecraft token gen fail %d!", ret);
    ESP_LOGI(TAG, "\n EUI:%s\n KEY:%s\n Token:%s", p_sensecraft->deviceinfo.eui, p_sensecraft->deviceinfo.key, p_sensecraft->https_token);

    p_sensecraft->sem_handle = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(NULL != p_sensecraft->sem_handle, ESP_ERR_NO_MEM, err, TAG, "Failed to create semaphore");

    p_sensecraft->net_sem_handle = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(NULL != p_sensecraft->net_sem_handle, ESP_ERR_NO_MEM, err, TAG, "Failed to create semaphore");

    p_sensecraft->p_task_stack_buf = (StackType_t *)psram_malloc(SENSECRAFT_TASK_STACK_SIZE);
    ESP_GOTO_ON_FALSE(NULL != p_sensecraft->p_task_stack_buf, ESP_ERR_NO_MEM, err, TAG, "Failed to malloc task stack");

    p_sensecraft->p_task_buf =  heap_caps_malloc(sizeof(StaticTask_t),  MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(NULL != p_sensecraft->p_task_buf, ESP_ERR_NO_MEM, err, TAG, "Failed to malloc task TCB");

    p_sensecraft->task_handle = xTaskCreateStatic(__sensecraft_task,
                                                "__sensecraft_task",
                                                SENSECRAFT_TASK_STACK_SIZE,
                                                (void *)p_sensecraft,
                                                SENSECRAFT_TASK_PRIO,
                                                p_sensecraft->p_task_stack_buf,
                                                p_sensecraft->p_task_buf);
    ESP_GOTO_ON_FALSE(p_sensecraft->task_handle, ESP_FAIL, err, TAG, "Failed to create task");


    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_SNTP_TIME_SYNCED,
                                                            __event_loop_handler, p_sensecraft, NULL));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle,
                                                             VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST,
                                                             __event_loop_handler, p_sensecraft, NULL));
    return ESP_OK;

err:
    if(p_sensecraft->task_handle ) {
        vTaskDelete(p_sensecraft->task_handle);
        p_sensecraft->task_handle = NULL;
    }
    if( p_sensecraft->p_task_stack_buf ) {
        free(p_sensecraft->p_task_stack_buf);
        p_sensecraft->p_task_stack_buf = NULL;
    }
    if( p_sensecraft->p_task_buf ) {
        free(p_sensecraft->p_task_buf);
        p_sensecraft->p_task_buf = NULL;
    }
    if (p_sensecraft->sem_handle) {
        vSemaphoreDelete(p_sensecraft->sem_handle);
        p_sensecraft->sem_handle = NULL;
    }
    if (p_sensecraft->net_sem_handle) {
        vSemaphoreDelete(p_sensecraft->net_sem_handle);
        p_sensecraft->net_sem_handle = NULL;
    }
    if (p_sensecraft) {
        free(p_sensecraft);
        p_sensecraft = NULL;
    }
    return ret;
}

esp_err_t app_sensecraft_https_token_get(char *p_token, size_t len)
{
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL || len != HTTPS_TOKEN_LEN ) {
        return ESP_FAIL;
    }
    memcpy(p_token, p_sensecraft->https_token, len);
    return ESP_OK;
}

esp_err_t app_sensecraft_https_token_gen(struct view_data_deviceinfo *p_deviceinfo, char *p_token, size_t len)
{
    esp_err_t ret = ESP_OK;
    size_t str_len = 0;
    size_t token_len = 0;
    char deviceinfo_buf[70];
    memset(deviceinfo_buf, 0, sizeof(deviceinfo_buf));
    str_len = snprintf(deviceinfo_buf, sizeof(deviceinfo_buf), "%s:%s", p_deviceinfo->eui, p_deviceinfo->key);
    ret = mbedtls_base64_encode(( uint8_t *)p_token, len, &token_len, ( uint8_t *)deviceinfo_buf, str_len);
    if( ret != 0  ||  token_len < 60 ) {
        ESP_LOGE(TAG, "mbedtls_base64_encode failed:%d,", ret);
        return ret;
    }
    return ESP_OK;
}

esp_err_t app_sensecraft_mqtt_report_taskflow(char *p_str, size_t len)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }
    const char *json_fmt =  \
    "{"
        "\"requestId\": \"%s\","
        "\"timestamp\": %jd,"
        "\"intent\": \"event\","
        "\"deviceEui\": \"%s\","
        "\"events\":  [{"
            "\"name\": \"change-device-status\","
            "\"value\": {"
                "\"3969\": %.*s"
            "},"
            "\"timestamp\": %jd"
        "}]"
    "}";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle != NULL, ESP_FAIL, TAG, "mqtt_client is not inited yet");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet");

    size_t json_buf_len = len + 512;
    char *json_buff = psram_malloc( json_buf_len );
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    char uuid[37];
    time_t timestamp_ms = util_get_timestamp_ms();

    UUIDGen(uuid);
    size_t json_len = sniprintf(json_buff, json_buf_len, json_fmt, uuid, timestamp_ms, \
                                    p_sensecraft->deviceinfo.eui, len, p_str, timestamp_ms);

    ESP_LOGD(TAG, "app_sensecraft_mqtt_report_taskflow: \r\n%s\r\nstrlen=%d", json_buff, json_len);

    int msg_id = esp_mqtt_client_enqueue(p_sensecraft->mqtt_handle, p_sensecraft->topic_up_change_device_status, json_buff, json_len,
                                        MQTT_PUB_QOS, false/*retain*/, true/*store*/);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "app_sensecraft_mqtt_report_taskflow enqueue failed, err=%d", msg_id);
        ret = ESP_FAIL;
    }

    free(json_buff);

    return ret;
}
esp_err_t app_sensecraft_mqtt_report_taskflow_ack(char *request_id, char *p_str, size_t len)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }
    const char *json_fmt =  \
    "{"
        "\"requestId\": \"%s\","
        "\"timestamp\": %jd,"
        "\"intent\": \"order\","
        "\"type\": \"response\","
        "\"deviceEui\": \"%s\","
        "\"order\":  ["
            "{"
                "\"name\": \"task-publish-ack\","
                "\"value\": {"
                    "\"code\":0,"
                    "\"data\": {"
                        "\"tl\": %.*s"
                    "}"
                "}"
            "}"
        "]"
    "}";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle != NULL, ESP_FAIL, TAG, "mqtt_client is not inited yet");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet");

    size_t json_buf_len = len + 512;
    char *json_buff = psram_malloc( json_buf_len );
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    time_t timestamp_ms = util_get_timestamp_ms();

    size_t json_len = sniprintf(json_buff, json_buf_len, json_fmt, request_id, timestamp_ms, \
                                    p_sensecraft->deviceinfo.eui, len, p_str);

    ESP_LOGD(TAG, "app_sensecraft_mqtt_report_taskflow_ack: \r\n%s\r\nstrlen=%d", json_buff, json_len);

    int msg_id = esp_mqtt_client_enqueue(p_sensecraft->mqtt_handle, p_sensecraft->topic_up_task_publish_ack, json_buff, json_len,
                                        MQTT_PUB_QOS, false/*retain*/, true/*store*/);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "app_sensecraft_mqtt_report_taskflow_ack enqueue failed, err=%d", msg_id);
        ret = ESP_FAIL;
    }

    free(json_buff);

    return ret;
}

esp_err_t app_sensecraft_mqtt_report_taskflow_status(intmax_t tasklist_id, int tf_status)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }
    const char *json_fmt =  \
    "{"
        "\"requestId\": \"%s\","
        "\"timestamp\": %jd,"
        "\"intent\": \"event\","
        "\"deviceEui\": \"%s\","
        "\"events\":  [{"
            "\"name\": \"change-device-status\","
            "\"value\": {"
                "\"3968\": {"
                    "\"tlid\": %jd,"
                    "\"status\": %d"
                "}"
            "},"
            "\"timestamp\": %jd"
        "}]"
    "}";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle, ESP_FAIL, TAG, "mqtt_client is not inited yet [2]");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet [2]");

    char *json_buff = psram_malloc(2048);
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    char uuid[37];
    time_t timestamp_ms = util_get_timestamp_ms();

    UUIDGen(uuid);
    size_t json_len = sniprintf(json_buff, 2048, json_fmt, uuid, timestamp_ms, p_sensecraft->deviceinfo.eui, tasklist_id, tf_status,
                    timestamp_ms);

    ESP_LOGD(TAG, "app_sensecraft_mqtt_report_taskflow_status: \r\n%s\r\nstrlen=%d", json_buff, json_len);

    int msg_id = esp_mqtt_client_enqueue(p_sensecraft->mqtt_handle, p_sensecraft->topic_up_change_device_status, json_buff, json_len,
                                        MQTT_PUB_QOS, false/*retain*/, true/*store*/);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "app_sensecraft_mqtt_report_taskflow_status enqueue failed, err=%d", msg_id);
        ret = ESP_FAIL;
    }

    free(json_buff);

    return ret;
}
esp_err_t app_sensecraft_mqtt_report_taskflow_module_status(intmax_t tasklist_id, 
                                                            int tf_status,  
                                                            char *p_module_name, int module_status)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }
    const char *json_fmt =  \
    "{"
        "\"requestId\": \"%s\","
        "\"timestamp\": %jd,"
        "\"intent\": \"event\","
        "\"deviceEui\": \"%s\","
        "\"events\":  [{"
            "\"name\": \"change-device-status\","
            "\"value\": {"
                "\"3968\": {"
                    "\"tlid\": %jd,"
                    "\"status\": %d"
                "},"
                "\"3970\":["
                    "{"
                        "\"name\": \"%s\","
                        "\"status\": %d"
                    "}"
                "],"
            "},"
            "\"timestamp\": %jd"
        "}]"
    "}";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle, ESP_FAIL, TAG, "mqtt_client is not inited yet [2]");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet [2]");

    char *json_buff = psram_malloc(2048);
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    char uuid[37];
    time_t timestamp_ms = util_get_timestamp_ms();

    UUIDGen(uuid);
    size_t json_len = sniprintf(json_buff, 2048, json_fmt, uuid, timestamp_ms, p_sensecraft->deviceinfo.eui, tasklist_id, tf_status,
                        p_module_name, module_status, timestamp_ms);

    ESP_LOGD(TAG, "app_sensecraft_mqtt_report_taskflow_module_status: \r\n%s\r\nstrlen=%d", json_buff, json_len);

    int msg_id = esp_mqtt_client_enqueue(p_sensecraft->mqtt_handle, p_sensecraft->topic_up_change_device_status, json_buff, json_len,
                                        MQTT_PUB_QOS, false/*retain*/, true/*store*/);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "app_sensecraft_mqtt_report_taskflow_module_status enqueue failed, err=%d", msg_id);
        ret = ESP_FAIL;
    }

    free(json_buff);

    return ret;
}


esp_err_t app_sensecraft_mqtt_report_warn_event(intmax_t taskflow_id, 
                                                char *taskflow_name, 
                                                char *p_img, size_t img_len, 
                                                char *p_msg, size_t msg_len)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }
    const char *json_fmt =  \
    "{"
        "\"requestId\": \"%s\","
        "\"timestamp\": %jd,"
        "\"intent\": \"event\","
        "\"deviceEui\": \"%s\","
        "\"events\":  [{"
            "\"name\": \"measure-sensor\","
            "\"value\": [{"
                "\"channel\": 1,"
                "\"measurements\": {"
                    "\"5004\": ["
                        "{"
                            "\"tlid\": %jd,"
                            "\"tn\": \"%s\","
                            "\"image\": \"%.*s\","
                            "\"content\": \"%.*s\""
                        "}"
                    "]"
                "},"
                "\"measureTime\": %jd"
            "}]"
        "}]"
    "}";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle, ESP_FAIL, TAG, "mqtt_client is not inited yet [3]");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet [3]");

    size_t json_buf_len = img_len + msg_len + 512;
    char *json_buff = psram_malloc( json_buf_len );
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    char uuid[37];
    time_t timestamp_ms = util_get_timestamp_ms();

    UUIDGen(uuid);
    size_t json_len = sniprintf(json_buff, json_buf_len, json_fmt, uuid, timestamp_ms, p_sensecraft->deviceinfo.eui,
              taskflow_id, taskflow_name, img_len, p_img, msg_len, p_msg, timestamp_ms);

    ESP_LOGD(TAG, "app_sensecraft_mqtt_report_warn_event: \r\n%s\r\nstrlen=%d", json_buff, json_len);

    int msg_id = esp_mqtt_client_enqueue(p_sensecraft->mqtt_handle, p_sensecraft->topic_up_warn_event_report, json_buff, json_len,
                                        MQTT_PUB_QOS, false/*retain*/, true/*store*/);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "app_sensecraft_mqtt_report_warn_event enqueue failed, err=%d", msg_id);
        ret = ESP_FAIL;
    }
    free(json_buff);

    return ret;
}

esp_err_t app_sensecraft_mqtt_report_device_status_generic(char *event_value_fields)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }
    const char *json_fmt =  \
    "{"
        "\"requestId\": \"%s\","
        "\"timestamp\": %jd,"
        "\"intent\": \"event\","
        "\"deviceEui\": \"%s\","
        "\"events\":  [{"
            "\"name\": \"change-device-status\","
            "\"value\": {%s},"
            "\"timestamp\": %jd"
        "}]"
    "}";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle, ESP_FAIL, TAG, "mqtt_client is not inited yet [4]");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet [4]");

    char *json_buff = psram_malloc(3000);
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    char uuid[37];
    time_t timestamp_ms = util_get_timestamp_ms();

    UUIDGen(uuid);
    sniprintf(json_buff, 3000, json_fmt, uuid, timestamp_ms, p_sensecraft->deviceinfo.eui, event_value_fields, timestamp_ms);

    ESP_LOGD(TAG, "app_sensecraft_mqtt_report_device_status: \r\n%s\r\nstrlen=%d", json_buff, strlen(json_buff));

    int msg_id = esp_mqtt_client_enqueue(p_sensecraft->mqtt_handle, p_sensecraft->topic_up_change_device_status, json_buff, strlen(json_buff),
                                        MQTT_PUB_QOS, false/*retain*/, true/*store*/);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "app_sensecraft_mqtt_report_device_status enqueue failed, err=%d", msg_id);
        ret = ESP_FAIL;
    }

    free(json_buff);

    return ret;
}

esp_err_t app_sensecraft_mqtt_report_device_status(struct view_data_device_status *dev_status)
{
    int ret = ESP_OK;
    struct app_sensecraft * p_sensecraft = gp_sensecraft;
    if( p_sensecraft == NULL) {
        return ESP_FAIL;
    }

    // please be carefull about the `comma` if you're appending more fields,
    // there should be NO trailing comma.
    const char *fields =  \
                "\"3000\": %d,"
                "\"3001\": \"%s\","
                "\"3002\": \"%s\","
                "\"3003\": \"%s\","
                "\"3502\": \"%s\"";

    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_handle, ESP_FAIL, TAG, "mqtt_client is not inited yet [4]");
    ESP_RETURN_ON_FALSE(p_sensecraft->mqtt_connected_flag, ESP_FAIL, TAG, "mqtt_client is not connected yet [4]");

    const int buff_sz = 2048;
    char *json_buff = psram_calloc(1, buff_sz);
    ESP_RETURN_ON_FALSE(json_buff != NULL, ESP_FAIL, TAG, "psram_malloc failed");

    sniprintf(json_buff, buff_sz, fields, 
              dev_status->battery_per, dev_status->hw_version, dev_status->fw_version, dev_status->fw_version, dev_status->fw_version);

    if (dev_status->himax_fw_version) {
        // himax version might be NULL, if NULL don't include 3577
        int len = strlen(json_buff);
        const char *field3577 = ",\"3577\": \"%s\"";
        sniprintf(json_buff + len, buff_sz - len, field3577, dev_status->himax_fw_version);
    }

    ret = app_sensecraft_mqtt_report_device_status_generic(json_buff);

    free(json_buff);

    return ret;
}