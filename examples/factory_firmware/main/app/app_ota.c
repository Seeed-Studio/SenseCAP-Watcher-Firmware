
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"

#include "sensecap-watcher.h"

#include "app_ota.h"
#include "data_defs.h"
#include "event_loops.h"
#include "util.h"

#define HTTPS_TIMEOUT_MS                30000
#define HTTPS_DOWNLOAD_RETRY_TIMES      5
#define SSCMA_FLASH_CHUNK_SIZE          128   //this value is copied from the `sscma_client_ota` example

enum {
    OTA_TYPE_ESP32 = 1,
    OTA_TYPE_HIMAX,
    OTA_TYPE_AI_MODEL
};


static const char *TAG = "ota";

static TaskHandle_t g_task;
static StaticTask_t g_task_tcb;
static bool g_ota_running = false;
static uint8_t network_connect_flag = 0;
static TaskHandle_t g_task_worker;

static SemaphoreHandle_t g_sem_network;
static SemaphoreHandle_t g_sem_ai_model_downloaded;
static esp_err_t g_result_err;
static char *g_url;


static int cmp_versions ( const char * version1, const char * version2 ) {
	unsigned major1 = 0, minor1 = 0, bugfix1 = 0;
	unsigned major2 = 0, minor2 = 0, bugfix2 = 0;
	sscanf(version1, "%u.%u.%u", &major1, &minor1, &bugfix1);
	sscanf(version2, "%u.%u.%u", &major2, &minor2, &bugfix2);
	if (major1 < major2) return -1;
	if (major1 > major2) return 1;
	if (minor1 < minor2) return -1;
	if (minor1 > minor2) return 1;
	if (bugfix1 < bugfix2) return -1;
	if (bugfix1 > bugfix2) return 1;
	return 0;
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "New firmware version: %s", new_app_info->version);

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t *running_app_info = psram_calloc(1, sizeof(esp_app_desc_t));
    if (esp_ota_get_partition_description(running, running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info->version);
    } else {
        ESP_LOGW(TAG, "Failed to get running_app_info! Always do OTA.");
    }

    int res = cmp_versions(new_app_info->version, running_app_info->version);
    free(running_app_info);

    if (res <= 0) return ESP_ERR_OTA_VERSION_TOO_OLD;

    return ESP_OK;
}

static esp_err_t __http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

static void esp32_ota_process()
{
    ESP_LOGI(TAG, "starting esp32 ota process, downloading %s", g_url);

    esp_err_t ota_finish_err = ESP_OK;

    esp_http_client_config_t *config = psram_calloc(1, sizeof(esp_http_client_config_t));
    config->url = g_url;
    config->crt_bundle_attach = esp_crt_bundle_attach;
    config->timeout_ms = HTTPS_TIMEOUT_MS;

    // don't enable this if the cert of your OTA server needs SNI support.
    // e.g. Your cert is generated with Let's Encrypt and the CommonName is *.yourdomain.com,
    // in this case, please don't enable this, SNI needs common name check.
#ifdef CONFIG_SKIP_COMMON_NAME_CHECK
    config->skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t *ota_config = psram_calloc(1, sizeof(esp_https_ota_config_t));
    ota_config->http_config = config;
    ota_config->http_client_init_cb = __http_client_init_cb;
    // ota_config->partial_http_download = true;
    // ota_config->max_http_request_size = MBEDTLS_SSL_IN_CONTENT_LEN;

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err;
    int i;
    struct view_data_ota_status ota_status;

    for (i = 0; i < HTTPS_DOWNLOAD_RETRY_TIMES; i++)
    {
        err = esp_https_ota_begin(ota_config, &https_ota_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp32 ota begin failed [%d]", i);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else break;
    }

    if (i >= HTTPS_DOWNLOAD_RETRY_TIMES) {
        ESP_LOGE(TAG, "esp32 ota begin failed eventually");
        ota_status.status = OTA_STATUS_FAIL;
        ota_status.err_code = ESP_ERR_OTA_CONNECTION_FAIL;
        esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW, 
                                    &ota_status, sizeof(struct view_data_ota_status),
                                    portMAX_DELAY);
        free(config);
        free(ota_config);
        return;
    }

    ESP_LOGI(TAG, "esp32 ota connection established, start downloading ...");
    ota_status.status = OTA_STATUS_DOWNLOADING;
    ota_status.percentage = 0;
    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW, 
                        &ota_status, sizeof(struct view_data_ota_status),
                        portMAX_DELAY);

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp32 ota, esp_https_ota_get_img_desc failed");
        ota_status.status = OTA_STATUS_FAIL;
        ota_status.err_code = ESP_ERR_OTA_GET_IMG_HEADER_FAIL;
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp32 ota, validate new firmware failed");
        ota_status.status = OTA_STATUS_FAIL;
        ota_status.err_code = err;
        goto ota_end;
    }

    int total_bytes, read_bytes = 0, last_report_bytes = 0;
    int step_bytes;

    total_bytes = esp_https_ota_get_image_size(https_ota_handle);
    ESP_LOGI(TAG, "New firmware binary length: %d", total_bytes);
    step_bytes = (int)(total_bytes / 10);
    last_report_bytes = step_bytes;

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        read_bytes = esp_https_ota_get_image_len_read(https_ota_handle);
        if (read_bytes >= last_report_bytes) {
            ota_status.status = OTA_STATUS_DOWNLOADING;
            ota_status.percentage = (int)(100 * read_bytes / total_bytes);
            ota_status.err_code = ESP_OK;
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW, 
                                &ota_status, sizeof(struct view_data_ota_status),
                                portMAX_DELAY);
            last_report_bytes += step_bytes;
            ESP_LOGI(TAG, "esp32 ota, image bytes read: %d, %d%%", read_bytes, ota_status.percentage);
        }
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "esp32 ota, complete data was not received.");
        ota_status.status = OTA_STATUS_FAIL;
        ota_status.err_code = ESP_ERR_OTA_DOWNLOAD_FAIL;
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "esp32 ota, upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            // TODO: call mqtt client to report ota status, better do blocking call
            esp_restart();
            //return;
        } else {
            ota_status.status = OTA_STATUS_FAIL;
            ota_status.err_code = ESP_ERR_OTA_DOWNLOAD_FAIL;
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "esp32 ota, image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "esp32 ota, upgrade failed when trying to finish: 0x%x", ota_finish_err);
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW, 
                        &ota_status, sizeof(struct view_data_ota_status),
                        portMAX_DELAY);
            free(config);
            free(ota_config);
            return;
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW, 
                        &ota_status, sizeof(struct view_data_ota_status),
                        portMAX_DELAY);
    free(config);
    free(ota_config);
}

static void __esp32_ota_worker_task(void *parg)
{
    ESP_LOGI(TAG, "starting esp32 ota worker task ...");

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  //task sleep
        ESP_LOGI(TAG, "esp32 ota worker task: start working ...");

        esp32_ota_process();
    }
}

static const char *ota_type_str(int ota_type)
{
    if (ota_type == OTA_TYPE_ESP32) return "esp32 firmware";
    else if (ota_type == OTA_TYPE_HIMAX) return "himax firmware";
    else if (ota_type == OTA_TYPE_AI_MODEL) return "ai model";
    else return "unknown ota";
}

static esp_err_t __http_event_handler(esp_http_client_event_t *evt)
{
    static int content_len, written_len, last_report_bytes, step_bytes;

    ota_http_userdata_t *userdata = evt->user_data;
    int ota_type = userdata->ota_type;
    struct view_data_ota_status ota_status;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            content_len = 0;
            written_len = 0;
            last_report_bytes = 0;
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGV(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            if (content_len == 0) {
                content_len = esp_http_client_get_content_length(evt->client);
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, content_len=%d", content_len);
                step_bytes = (int)(content_len / 10);
                last_report_bytes = step_bytes;
            }

            if (sscma_client_ota_write(userdata->client, evt->data, evt->data_len) != ESP_OK)
            {
                ESP_LOGW(TAG, "sscma_client_ota_write failed\n");
                *(userdata->err) = ESP_ERR_OTA_SSCMA_WRITE_FAIL;
            } else {
                written_len += evt->data_len;
                if (written_len >= last_report_bytes) {
                    ota_status.status = OTA_STATUS_DOWNLOADING;
                    ota_status.percentage = (int)(100 * written_len / content_len);
                    ota_status.err_code = ESP_OK;
                    int32_t eventid = ota_type == OTA_TYPE_HIMAX ? CTRL_EVENT_OTA_HIMAX_FW: CTRL_EVENT_OTA_AI_MODEL;
                    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, eventid, 
                                        &ota_status, sizeof(struct view_data_ota_status),
                                        portMAX_DELAY);
                    last_report_bytes += step_bytes;
                    ESP_LOGI(TAG, "%s ota, bytes written: %d, %d%%", ota_type_str(ota_type), written_len, ota_status.percentage);
                }
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            sscma_client_ota_finish(userdata->client);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT, auto redirection disabled?");
            break;
    }
    return ESP_OK;
}

static void sscma_ota_process(uint32_t ota_type)
{
    ESP_LOGI(TAG, "starting sscma ota, ota_type = %s ...", ota_type_str(ota_type));

    esp_err_t ret = ESP_OK;

    //himax interfaces
    esp_io_expander_handle_t io_expander = bsp_io_expander_init();
    assert(io_expander != NULL);

    sscma_client_handle_t sscma_client = bsp_sscma_client_init();
    assert(sscma_client != NULL);

    sscma_client_flasher_handle_t sscma_flasher = bsp_sscma_flasher_init();
    assert(sscma_flasher != NULL);

    sscma_client_init(sscma_client);

    sscma_client_info_t *info;
    if (sscma_client_get_info(sscma_client, &info, true) == ESP_OK)
    {
        ESP_LOGI(TAG, "--------------------------------------------");
        ESP_LOGI(TAG, "           sscma client info");
        ESP_LOGI(TAG, "ID: %s", (info->id != NULL) ? info->id : "NULL");
        ESP_LOGI(TAG, "Name: %s", (info->name != NULL) ? info->name : "NULL");
        ESP_LOGI(TAG, "Hardware Version: %s", (info->hw_ver != NULL) ? info->hw_ver : "NULL");
        ESP_LOGI(TAG, "Software Version: %s", (info->sw_ver != NULL) ? info->sw_ver : "NULL");
        ESP_LOGI(TAG, "Firmware Version: %s", (info->fw_ver != NULL) ? info->fw_ver : "NULL");
        ESP_LOGI(TAG, "--------------------------------------------");
    }
    else
    {
        ESP_LOGW(TAG, "sscma client get info failed\n");
    }

    int64_t start = esp_timer_get_time();
    uint32_t flash_addr = 0x0;
    if (ota_type == OTA_TYPE_HIMAX) {
        ESP_LOGI(TAG, "flash Himax firmware ...");
    } else {
        ESP_LOGI(TAG, "flash Himax 4th ai model ...");
        flash_addr = 0xA00000;
    }

    if (sscma_client_ota_start(sscma_client, sscma_flasher, flash_addr) != ESP_OK)
    {
        ESP_LOGI(TAG, "sscma_client_ota_start failed\n");
        g_result_err = ESP_ERR_OTA_SSCMA_START_FAIL;
        return;
    }

    //build the bridge struct
    esp_err_t err_in_http_events = ESP_OK;
    ota_http_userdata_t ota_http_userdata = {
        .client = sscma_client,
        .flasher = sscma_flasher,
        .ota_type = (int)ota_type,
        .err = &err_in_http_events
    };

    //https init
    esp_http_client_config_t *http_client_config = NULL;
    esp_http_client_handle_t http_client = NULL;

    http_client_config = psram_calloc(1, sizeof(esp_http_client_config_t));
    ESP_GOTO_ON_FALSE(http_client_config != NULL, ESP_ERR_NO_MEM, sscma_ota_end,
                      TAG, "sscma ota, mem alloc fail [1]");
    http_client_config->url = g_url;
    http_client_config->method = HTTP_METHOD_GET;
    http_client_config->timeout_ms = HTTPS_TIMEOUT_MS;
    http_client_config->crt_bundle_attach = esp_crt_bundle_attach;
    http_client_config->user_data = &ota_http_userdata;
    http_client_config->buffer_size = SSCMA_FLASH_CHUNK_SIZE;
    http_client_config->event_handler = __http_event_handler;
#ifdef CONFIG_SKIP_COMMON_NAME_CHECK
    http_client_config->skip_cert_common_name_check = true;
#endif

    http_client = esp_http_client_init(http_client_config);
    ESP_GOTO_ON_FALSE(http_client != NULL, ESP_ERR_OTA_CONNECTION_FAIL, sscma_ota_end,
                      TAG, "sscma ota, http client init fail");
    
    esp_err_t err = esp_http_client_perform(http_client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "sscma ota, HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(http_client),
                esp_http_client_get_content_length(http_client));
        if (err_in_http_events != ESP_OK) {
            ret = err_in_http_events;
        } else {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "%s ota success, take %lld us\n", ota_type_str(ota_type), esp_timer_get_time() - start);
        }
    } else {
        ESP_LOGE(TAG, "sscma ota, HTTP GET request failed: %s", esp_err_to_name(err));
        //error defines:
        //https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.1/esp32s3/api-reference/protocols/esp_http_client.html#macros
        ret = ESP_ERR_OTA_DOWNLOAD_FAIL;  //we sum all these errors as download failure, easier for upper caller
    }

sscma_ota_end:
    if (http_client_config) free(http_client_config);
    if (http_client) esp_http_client_cleanup(http_client);

    g_result_err = ret;
}

static void __app_ota_task(void *p_arg)
{
    uint32_t ota_type;

    ESP_LOGI(TAG, "starting ota task ...");

    while (1) {
        // wait for network connection
        xSemaphoreTake(g_sem_network, pdMS_TO_TICKS(10000));
        if (!network_connect_flag)
        {
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(3000));  //give the time to more important tasks right after the network is established

        ESP_LOGI(TAG, "network established, waiting for OTA request ...");

        do {
            ota_type = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            g_ota_running = true;
            if (ota_type == OTA_TYPE_ESP32) {
                xTaskNotifyGive(g_task_worker);  // wakeup the task
            } else if (ota_type == OTA_TYPE_HIMAX) {

            } else if (ota_type == OTA_TYPE_AI_MODEL) {
                sscma_ota_process(OTA_TYPE_AI_MODEL);
                xSemaphoreGive(g_sem_ai_model_downloaded);
            } else {
                ESP_LOGW(TAG, "unknown ota type: %" PRIu32, ota_type);
            }
            g_ota_running = false;

        } while (network_connect_flag);
    }
}

/* Event handler for catching system events */
static void __sys_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "ota event: OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "ota event: Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "ota event: Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "ota event: Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "ota event: Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGV(TAG, "ota event: Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "ota event: Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "ota event: OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "ota event: OTA abort");
                break;
        }
    }
    else if (event_base == ESP_HTTP_CLIENT_EVENT) {
        switch (event_id) {
            case HTTP_EVENT_REDIRECT:
                ESP_LOGI(TAG, "http event: Redirection");
                break;
            default:
                break;
        }
    }
}

static void __app_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    //wifi connection state changed
    case VIEW_EVENT_WIFI_ST:
    {
        ESP_LOGI(TAG, "event: VIEW_EVENT_WIFI_ST");
        struct view_data_wifi_st *p_st = (struct view_data_wifi_st *)event_data;
        network_connect_flag = p_st->is_network ? 1 : 0;
        xSemaphoreGive(g_sem_network);
        break;
    }
    default:
        break;
    }
}

#if CONFIG_ENABLE_TEST_ENV
static void __ota_test_task(void *p_arg)
{
    while (!network_connect_flag) {
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    vTaskDelay(pdMS_TO_TICKS(3000));

    // esp_err_t res = app_ota_esp32_fw_download("https://new.pxspeed.site/factory_firmware.bin");
    // ESP_LOGI(TAG, "test app_ota_esp32_fw_download: 0x%x", res);

    // esp_err_t res = app_ota_ai_model_download("https://new.pxspeed.site/human_pose.tflite", 0);
    // ESP_LOGI(TAG, "test app_ota_ai_model_download: 0x%x", res);

    vTaskDelete(NULL);
}
#endif

esp_err_t app_ota_init(void)
{
#if CONFIG_ENABLE_FACTORY_FW_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif

    g_sem_network = xSemaphoreCreateBinary();
    g_sem_ai_model_downloaded = xSemaphoreCreateBinary();

    // esp32 ota worker task, due to PSRAM limitations, we need a task with internal RAM as stack
    xTaskCreate(__esp32_ota_worker_task, "esp32_ota_worker", 1024 * 3, NULL, 1, &g_task_worker);

    // ota main task
    const uint32_t stack_size = 10 * 1024;
    StackType_t *task_stack = (StackType_t *)psram_calloc(1, stack_size);
    g_task = xTaskCreateStatic(__app_ota_task, "app_ota", stack_size, NULL, 1, task_stack, &g_task_tcb);

    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, __sys_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTP_CLIENT_EVENT, ESP_EVENT_ANY_ID, __sys_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST,
                                                             __app_event_handler, NULL, NULL));

#if CONFIG_ENABLE_TEST_ENV
    StackType_t *task_stack2 = (StackType_t *)psram_calloc(1, stack_size);
    StaticTask_t *task_tcb = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    xTaskCreateStatic(__ota_test_task, "ota_test", stack_size, NULL, 1, task_stack2, task_tcb);
#endif

    return ESP_OK;
}

esp_err_t app_ota_ai_model_download(char *url, int size_bytes)
{
    if (g_ota_running) return ESP_ERR_OTA_ALREADY_RUNNING;

    g_url = url;
    g_result_err = ESP_OK;

    if (xTaskNotify(g_task, OTA_TYPE_AI_MODEL, eSetValueWithoutOverwrite) == pdFAIL) {
        return ESP_ERR_OTA_ALREADY_RUNNING;
    }

    // block until ai model download completed or failed
    xSemaphoreTake(g_sem_ai_model_downloaded, portMAX_DELAY);

    return g_result_err;
}

esp_err_t app_ota_esp32_fw_download(char *url)
{
    if (g_ota_running) return ESP_ERR_OTA_ALREADY_RUNNING;

    g_url = url;

    if (xTaskNotify(g_task, OTA_TYPE_ESP32, eSetValueWithoutOverwrite) == pdFAIL) {
        return ESP_ERR_OTA_ALREADY_RUNNING;
    }

    return ESP_OK;
}

esp_err_t app_ota_himax_fw_download(char *url)
{
    if (g_ota_running) return ESP_ERR_OTA_ALREADY_RUNNING;

    return ESP_OK;
}
