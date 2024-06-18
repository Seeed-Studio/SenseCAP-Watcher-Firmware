
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

#include "sensecap-watcher.h"

#include "app_ota.h"
#include "data_defs.h"
#include "event_loops.h"
#include "util.h"
#include "tf_module_ai_camera.h"
#include "app_sensecraft.h"


ESP_EVENT_DEFINE_BASE(OTA_EVENT_BASE);


#define HTTPS_TIMEOUT_MS                30000
#define HTTPS_DOWNLOAD_RETRY_TIMES      5
#define HTTP_RX_CHUNK_SIZE              512
#define SSCMA_FLASH_CHUNK_SIZE_SPI      256   //this value is copied from the `sscma_client_ota` example
#define SSCMA_FLASH_CHUNK_SIZE_UART     128   //this value is copied from the `sscma_client_ota` example
#define AI_MODEL_RINGBUFF_SIZE          102400

//event group events
#define EVENT_OTA_STOP_BY_TOO_SLOW      BIT0
#define EVENT_OTA_HIMAX_HTTP_GOING      BIT1
#define EVENT_AI_MODEL_DL_PREPARING     BIT2
#define EVENT_AI_MODEL_DL_EARLY_ABORT   BIT3

enum {
    OTA_TYPE_ESP32 = 1,
    OTA_TYPE_HIMAX,
    OTA_TYPE_AI_MODEL
};

static const char *TAG = "ota";

static TaskHandle_t g_task;
static TaskHandle_t g_task_sscma_writer;
static StaticTask_t g_task_tcb;
static volatile atomic_bool g_ota_running = ATOMIC_VAR_INIT(false);
static volatile atomic_bool g_ignore_version_check = ATOMIC_VAR_INIT(false);
static volatile atomic_bool g_sscma_writer_abort = ATOMIC_VAR_INIT(false);
static volatile atomic_bool g_network_connected_flag = ATOMIC_VAR_INIT(false);
static volatile atomic_bool g_mqtt_connected_flag = ATOMIC_VAR_INIT(false);

static SemaphoreHandle_t g_sem_network;
static SemaphoreHandle_t g_sem_ai_model_downloaded;
static SemaphoreHandle_t g_sem_worker_done;
static SemaphoreHandle_t g_sem_sscma_writer_done;
static EventGroupHandle_t g_eg_globalsync;
static esp_err_t g_result_err;
static char *g_cur_ota_version_esp32;
static char *g_cur_ota_version_himax;

static QueueHandle_t g_Q_ota_job;
static QueueHandle_t g_Q_ota_msg;
static QueueHandle_t g_Q_ota_status;
static RingbufHandle_t g_rb_ai_model;


static ota_sscma_writer_userdata_t g_sscma_writer_userdata;


static void __ota_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    ota_worker_task_data_t *worker_data = *(ota_worker_task_data_t **)event_data;

    switch(id) {
        case CMD_esp_https_ota_begin:
            worker_data->err = esp_https_ota_begin(worker_data->ota_config, worker_data->ota_handle);
            break;
        case CMD_esp_https_ota_get_img_desc:
            worker_data->err = esp_https_ota_get_img_desc(*(worker_data->ota_handle), worker_data->app_desc);
            break;
        case CMD_esp_ota_get_running_partition:
            worker_data->partition = esp_ota_get_running_partition();
            break;
        case CMD_esp_ota_get_partition_description:
            worker_data->err = esp_ota_get_partition_description(worker_data->partition, worker_data->app_desc);
            break;
        case CMD_esp_https_ota_perform:
            // this call may block the eventloop for quite a while (30sec), but since the device is showing the OTA UI,
            // the background activities are not sensitive to the users.
            // ESP_LOGD(TAG, "worker_call: esp_https_ota_perform");
            worker_data->err = esp_https_ota_perform(*(worker_data->ota_handle));
            // ESP_LOGD(TAG, "worker_call: esp_https_ota_perform done, 0x%x", worker_data->err);
            break;
        case CMD_esp_https_ota_finish:
            worker_data->err = esp_https_ota_finish(*(worker_data->ota_handle));
            break;
        default:
            break;
    }

    xSemaphoreGive(g_sem_worker_done);
}

static void worker_call(ota_worker_task_data_t *worker_data, int cmd)
{
    if(esp_event_post_to(app_event_loop_handle, OTA_EVENT_BASE, cmd,
                      &worker_data, sizeof(ota_worker_task_data_t *),  pdMS_TO_TICKS(10000)) != ESP_OK)
    {
        ESP_LOGW(TAG, "worker_call failed, maybe eventloop full?");
        worker_data->err = ESP_ERR_OTA_WORKERCALL_ERR;
        return;
    } 
    xSemaphoreTake(g_sem_worker_done, portMAX_DELAY);
}

static int cmp_versions ( const char * version1, const char * version2 ) {
	unsigned major1 = 0, minor1 = 0, bugfix1 = 0;
	unsigned major2 = 0, minor2 = 0, bugfix2 = 0;
    if (atomic_load(&g_ignore_version_check)) return 1;
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

    ota_worker_task_data_t worker_data;

    worker_call(&worker_data, CMD_esp_ota_get_running_partition);

    //esp_partition_t *running = worker_data.partition;
    esp_app_desc_t *running_app_info = psram_calloc(1, sizeof(esp_app_desc_t));

    worker_data.app_desc = running_app_info;
    worker_call(&worker_data, CMD_esp_ota_get_partition_description);

    if (worker_data.err == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info->version);
    } else {
        ESP_LOGW(TAG, "Failed to get running_app_info! Always do OTA.");
    }

    int res = cmp_versions(new_app_info->version, running_app_info->version);
    free(running_app_info);

    if (res <= 0) return ESP_ERR_OTA_VERSION_TOO_OLD;

    return ESP_OK;
}

static void esp32_ota_process(char *url)
{
    ESP_LOGI(TAG, "starting esp32 ota process, downloading %s", url);

    esp_err_t ota_finish_err = ESP_OK;
    ota_worker_task_data_t worker_data;

    esp_http_client_config_t *config = psram_calloc(1, sizeof(esp_http_client_config_t));
    config->url = url;
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

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err;
    int i;
    struct view_data_ota_status ota_status;

    for (i = 0; i < HTTPS_DOWNLOAD_RETRY_TIMES; i++)
    {
        worker_data.ota_config = ota_config;
        worker_data.ota_handle = &https_ota_handle;
        worker_call(&worker_data, CMD_esp_https_ota_begin);

        err = worker_data.err;
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp32 ota begin failed [%d]", i);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else break;
    }

    if (i >= HTTPS_DOWNLOAD_RETRY_TIMES) {
        ESP_LOGE(TAG, "esp32 ota begin failed eventually");
        ota_status.status = OTA_STATUS_FAIL;
        ota_status.err_code = ESP_ERR_OTA_CONNECTION_FAIL;
        esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW,
                                    &ota_status, sizeof(struct view_data_ota_status),
                                    pdMS_TO_TICKS(10000));
        free(config);
        free(ota_config);
        return;
    }

    ESP_LOGI(TAG, "esp32 ota connection established, start downloading ...");
    ota_status.status = OTA_STATUS_DOWNLOADING;
    ota_status.err_code = ESP_OK;
    ota_status.percentage = 0;
    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW,
                        &ota_status, sizeof(struct view_data_ota_status),
                        pdMS_TO_TICKS(10000));

    esp_app_desc_t app_desc;
    worker_data.ota_handle = &https_ota_handle;
    worker_data.app_desc = &app_desc;
    worker_call(&worker_data, CMD_esp_https_ota_get_img_desc);

    err = worker_data.err;
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
        worker_data.ota_handle = &https_ota_handle;
        worker_call(&worker_data, CMD_esp_https_ota_perform);
        err = worker_data.err;
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        if (xEventGroupWaitBits(g_eg_globalsync, EVENT_OTA_STOP_BY_TOO_SLOW, pdTRUE, pdTRUE, 0) & EVENT_OTA_STOP_BY_TOO_SLOW)
            break;

        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        read_bytes = esp_https_ota_get_image_len_read(https_ota_handle);
        if (read_bytes >= last_report_bytes) {
            ota_status.status = OTA_STATUS_DOWNLOADING;
            ota_status.percentage = (int)(100 * read_bytes / total_bytes);
            ota_status.err_code = ESP_OK;
            esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW,
                                &ota_status, sizeof(struct view_data_ota_status),
                                pdMS_TO_TICKS(10000));
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
        // goto ota_end;

        worker_data.ota_handle = &https_ota_handle;
        worker_call(&worker_data, CMD_esp_https_ota_finish);
        ota_finish_err = worker_data.err;
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "esp32 ota, upgrade successful. Rebooting ...");
            //vTaskDelay(1000 / portTICK_PERIOD_MS);
            // TODO: call mqtt client to report ota status, better do blocking call
            //esp_restart();
            //return;
            ota_status.status = OTA_STATUS_SUCCEED;
            ota_status.err_code = ESP_OK;
            ota_status.percentage = 100;
        } else {
            ota_status.status = OTA_STATUS_FAIL;
            ota_status.err_code = ESP_ERR_OTA_DOWNLOAD_FAIL;
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "esp32 ota, image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "esp32 ota, upgrade failed when trying to finish: 0x%x", ota_finish_err);
        }
    }

ota_end:
    if (ota_status.status != OTA_STATUS_SUCCEED) {
        esp_https_ota_abort(https_ota_handle);
    }
    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW,
                        &ota_status, sizeof(struct view_data_ota_status),
                        pdMS_TO_TICKS(10000));
    free(config);
    free(ota_config);
}

static const char *ota_type_str(int ota_type)
{
    if (ota_type == OTA_TYPE_ESP32) return "esp32 firmware";
    else if (ota_type == OTA_TYPE_HIMAX) return "himax firmware";
    else if (ota_type == OTA_TYPE_AI_MODEL) return "ai model";
    else return "unknown ota";
}

static sscma_client_flasher_handle_t bsp_sscma_flasher_init_legacy(sscma_client_handle_t sscma_client)
{
    static sscma_client_flasher_handle_t _sscma_flasher_handle = NULL;
    static sscma_client_io_handle_t _sscma_flasher_io_handle = NULL;

    static bool initialized = false;
    if (initialized)
        return _sscma_flasher_handle;

    if (bsp_io_expander_init() == NULL)
        return NULL;

    uart_config_t uart_config = {
        .baud_rate = BSP_SSCMA_FLASHER_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(BSP_SSCMA_FLASHER_UART_NUM, 64 * 1024, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(BSP_SSCMA_FLASHER_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(BSP_SSCMA_FLASHER_UART_NUM, BSP_SSCMA_FLASHER_UART_TX, BSP_SSCMA_FLASHER_UART_RX, -1, -1));

    sscma_client_io_uart_config_t io_uart_config = {
        .user_ctx = NULL,
    };

    sscma_client_new_io_uart_bus((sscma_client_uart_bus_handle_t)BSP_SSCMA_FLASHER_UART_NUM, &io_uart_config, &_sscma_flasher_io_handle);

    const sscma_client_flasher_we2_config_t flasher_config = {
        .reset_gpio_num = BSP_SSCMA_CLIENT_RST,
        .io_expander = sscma_client->io_expander,
        .flags.reset_use_expander = BSP_SSCMA_CLIENT_RST_USE_EXPANDER,
        .flags.reset_high_active = false,
        .user_ctx = NULL,
    };

    sscma_client_new_flasher_we2_uart(_sscma_flasher_io_handle, &flasher_config, &_sscma_flasher_handle);

    initialized = true;

    return _sscma_flasher_handle;
}

static void __sscma_writer_task(void *p_arg)
{
    ota_sscma_writer_userdata_t *userdata = &g_sscma_writer_userdata;
    int content_len, ota_type;
    struct view_data_ota_status ota_status;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        content_len = userdata->content_len;
        ota_type = userdata->ota_type;
        if (content_len <= 0) continue;

        ESP_LOGI(TAG, "starting sscma writer, content_len=%d ...", content_len);

        int32_t ota_eventid = ota_type == OTA_TYPE_HIMAX ? CTRL_EVENT_OTA_HIMAX_FW: CTRL_EVENT_OTA_AI_MODEL;

        //himax interfaces
        sscma_client_handle_t sscma_client = bsp_sscma_client_init();
        assert(sscma_client != NULL);

        bool use_spi_flasher = false;

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

            if (ota_type == OTA_TYPE_AI_MODEL && cmp_versions(info->fw_ver, "2024.06.03") >= 0) {
                ESP_LOGI(TAG, "sscma writer, will use SPI flasher");
                use_spi_flasher = true;
            }
        }
        else
        {
            ESP_LOGW(TAG, "sscma client get info failed, will try to flash anyway\n");
            // userdata->err = ESP_ERR_OTA_SSCMA_START_FAIL;
            // goto sscma_writer_end;
        }

        sscma_client_flasher_handle_t sscma_flasher = use_spi_flasher ? bsp_sscma_flasher_init() : 
                                                                        bsp_sscma_flasher_init_legacy(sscma_client);
        assert(sscma_flasher != NULL);

        int sscma_flasher_chunk_size_decided = use_spi_flasher ? SSCMA_FLASH_CHUNK_SIZE_SPI : SSCMA_FLASH_CHUNK_SIZE_UART;

        //sscma_client_init(sscma_client);

        int64_t start = esp_timer_get_time();
        uint32_t flash_addr = 0x0;
        if (ota_type == OTA_TYPE_HIMAX) {
            ESP_LOGI(TAG, "flash Himax firmware ...");
        } else {
            ESP_LOGI(TAG, "flash Himax 4th ai model ...");
            flash_addr = 0xA00000;
        }

        //sscma_client_ota_start
        if (sscma_client_ota_start(sscma_client, sscma_flasher, flash_addr) != ESP_OK) {
            ESP_LOGE(TAG, "sscma writer, sscma_client_ota_start failed");
            userdata->err = ESP_ERR_OTA_SSCMA_START_FAIL;
            goto sscma_writer_end;
        }
        ota_status.status = OTA_STATUS_DOWNLOADING;
        ota_status.err_code = ESP_OK;
        ota_status.percentage = 0;
        esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, ota_eventid, 
                                            &ota_status, sizeof(struct view_data_ota_status),
                                            pdMS_TO_TICKS(10000));

        // drain the ringbuffer and write to himax
        int written_len = 0;
        int remain_len = content_len - written_len;
        int step_bytes = (int)(content_len / 10);
        int last_report_bytes = step_bytes;
        int target_bytes;
        int retry_cnt = 0;
        void *chunk = psram_calloc(1, sscma_flasher_chunk_size_decided);
        void *tmp;
        UBaseType_t available_bytes = 0;
        size_t rcvlen, rcvlen2;

        while (remain_len > 0 && !atomic_load(&g_sscma_writer_abort)) {
            target_bytes = MIN(sscma_flasher_chunk_size_decided, remain_len);
            vRingbufferGetInfo(g_rb_ai_model, NULL, NULL, NULL, NULL, &available_bytes);
            if ((int)available_bytes < target_bytes) {
                vTaskDelay(pdMS_TO_TICKS(1));
                retry_cnt++;
                if (retry_cnt > 60000) {
                    ESP_LOGE(TAG, "sscma writer reach timeout on ringbuffer!!! want_bytes: %d", target_bytes);
                    userdata->err = ESP_ERR_OTA_SSCMA_WRITE_FAIL;
                    goto sscma_writer_end0;
                }
                continue;
            }
            retry_cnt = 0;

            rcvlen = 0, rcvlen2 = 0;
            tmp = xRingbufferReceiveUpTo(g_rb_ai_model, &rcvlen, 0, target_bytes);
            if (!tmp) {
                ESP_LOGW(TAG, "sscma writer, ringbuffer insufficient? this should never happen!");
                userdata->err = ESP_ERR_OTA_SSCMA_INTERNAL_ERR;
                goto sscma_writer_end0;
            }
            memset(chunk, 0, sscma_flasher_chunk_size_decided);
            memcpy(chunk, tmp, rcvlen);
            target_bytes -= rcvlen;
            vRingbufferReturnItem(g_rb_ai_model, tmp);
            //rollover?
            if (target_bytes > 0) {
                tmp = xRingbufferReceiveUpTo(g_rb_ai_model, &rcvlen2, 0, target_bytes);  //receive the 2nd part
                if (!tmp) {
                    ESP_LOGW(TAG, "sscma writer, ringbuffer insufficient? this should never happen [2]!");
                    userdata->err = ESP_ERR_OTA_SSCMA_INTERNAL_ERR;
                    goto sscma_writer_end0;
                }
                memcpy(chunk + rcvlen, tmp, rcvlen2);
                target_bytes -= rcvlen2;
                vRingbufferReturnItem(g_rb_ai_model, tmp);
            }

            if (target_bytes != 0) {
                ESP_LOGW(TAG, "sscma writer, this really should never happen!");
                userdata->err = ESP_ERR_OTA_SSCMA_INTERNAL_ERR;
                goto sscma_writer_end0;
            }

            //write to sscma client
            // ESP_LOGD(TAG, "sscma writer, sscma_client_ota_write");
            if (sscma_client_ota_write(sscma_client, chunk, sscma_flasher_chunk_size_decided) != ESP_OK)
            {
                ESP_LOGW(TAG, "sscma writer, sscma_client_ota_write failed\n");
                userdata->err = ESP_ERR_OTA_SSCMA_WRITE_FAIL;
                goto sscma_writer_end0;
            } else {
                written_len += (rcvlen + rcvlen2);
                if (written_len >= last_report_bytes) {
                    ota_status.status = OTA_STATUS_DOWNLOADING;
                    ota_status.percentage = (int)(100 * written_len / content_len);
                    ota_status.err_code = ESP_OK;
                    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, ota_eventid,
                                        &ota_status, sizeof(struct view_data_ota_status),
                                        pdMS_TO_TICKS(10000));
                    last_report_bytes += step_bytes;
                    ESP_LOGI(TAG, "%s ota, bytes written: %d, %d%%", ota_type_str(ota_type), written_len, ota_status.percentage);
                }
            }

            remain_len -= (rcvlen + rcvlen2);
        }  //while

        if (atomic_load(&g_sscma_writer_abort)) {
            sscma_client_ota_abort(sscma_client);
            ESP_LOGW(TAG, "%s sscma writer, abort, take %lld us", ota_type_str(ota_type), esp_timer_get_time() - start);
        } else {
            ESP_LOGD(TAG, "%s sscma writer, write done, take %lld us", ota_type_str(ota_type), esp_timer_get_time() - start);
            sscma_client_ota_finish(sscma_client);
            ESP_LOGI(TAG, "%s sscma writer, finish, take %lld us, speed %d KB/s", ota_type_str(ota_type), esp_timer_get_time() - start,
                            (int)(1000 * content_len / (esp_timer_get_time() - start)));
        }
sscma_writer_end0:
        free(chunk);
sscma_writer_end:
        xSemaphoreGive(g_sem_sscma_writer_done);
    }
}

static esp_err_t __http_event_handler(esp_http_client_event_t *evt)
{
    static int content_len, written_len, step_bytes, last_report_bytes;
    ota_sscma_writer_userdata_t *userdata = evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            content_len = 0;
            written_len = 0;
            //clear the ringbuffer
            void *tmp;
            size_t len;
            while ((tmp = xRingbufferReceiveUpTo(g_rb_ai_model, &len, 0, AI_MODEL_RINGBUFF_SIZE))) {
                vRingbufferReturnItem(g_rb_ai_model, tmp);
            }
            atomic_store(&g_sscma_writer_abort, false);
            xSemaphoreTake(g_sem_sscma_writer_done, 0);  //clear the semaphore if there's dirty one

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
                userdata->content_len = content_len;
                userdata->err = ESP_OK;
                xTaskNotifyGive(g_task_sscma_writer);

                step_bytes = (int)(content_len / 10);
                last_report_bytes = step_bytes;
            }

            if (userdata->err != ESP_OK) {
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, userdata->err != ESP_OK (0x%x), error happened in sscma writer", userdata->err);
                break;  // don't waste time on ringbuffer send
            }

            //push to ringbuffer
            //himax will move the written bytes into flash every 1MB, it will take pretty long.
            //we give it 1min?
            if (xRingbufferSend(g_rb_ai_model, evt->data, evt->data_len, pdMS_TO_TICKS(60000)) != pdTRUE) {
                ESP_LOGW(TAG, "HTTP_EVENT_ON_DATA, ringbuffer full? this should never happen!");
            }
            written_len += evt->data_len;
            if (written_len >= last_report_bytes) {
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, http get len: %d, %d%%", written_len, (int)(100 * written_len / content_len));
                last_report_bytes += step_bytes;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
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

static void sscma_ota_process(uint32_t ota_type, char *url)
{
    ESP_LOGI(TAG, "starting sscma ota, ota_type = %s ...", ota_type_str(ota_type));

    esp_err_t ret = ESP_OK;
    struct view_data_ota_status ota_status;
    int32_t ota_eventid = ota_type == OTA_TYPE_HIMAX ? CTRL_EVENT_OTA_HIMAX_FW: CTRL_EVENT_OTA_AI_MODEL;

    int64_t start = esp_timer_get_time();

    //https init
    esp_http_client_config_t *http_client_config = NULL;
    esp_http_client_handle_t http_client = NULL;

    http_client_config = psram_calloc(1, sizeof(esp_http_client_config_t));
    ESP_GOTO_ON_FALSE(http_client_config != NULL, ESP_ERR_NO_MEM, sscma_ota_end,
                      TAG, "sscma ota, mem alloc fail [1]");
    http_client_config->url = url;
    http_client_config->method = HTTP_METHOD_GET;
    http_client_config->timeout_ms = HTTPS_TIMEOUT_MS;
    http_client_config->crt_bundle_attach = esp_crt_bundle_attach;
    http_client_config->user_data = &g_sscma_writer_userdata;
    http_client_config->buffer_size = HTTP_RX_CHUNK_SIZE;
    http_client_config->event_handler = __http_event_handler;
#ifdef CONFIG_SKIP_COMMON_NAME_CHECK
    http_client_config->skip_cert_common_name_check = true;
#endif

    http_client = esp_http_client_init(http_client_config);
    ESP_GOTO_ON_FALSE(http_client != NULL, ESP_ERR_OTA_CONNECTION_FAIL, sscma_ota_end,
                      TAG, "sscma ota, http client init fail");

    g_sscma_writer_userdata.ota_type = ota_type;
    g_sscma_writer_userdata.http_client = http_client;
    g_sscma_writer_userdata.err = ESP_OK;

    //breakpoint to check if user canceled, this is the last chance to do early abortion
    if (xEventGroupGetBits(g_eg_globalsync) & EVENT_AI_MODEL_DL_EARLY_ABORT) {
        ret = ESP_ERR_OTA_USER_CANCELED;
        goto sscma_ota_end;
    }
    xEventGroupClearBits(g_eg_globalsync, EVENT_AI_MODEL_DL_PREPARING); //clear this bit regardless ai model dl or himax fw dl

    xEventGroupSetBits(g_eg_globalsync, EVENT_OTA_HIMAX_HTTP_GOING);
    esp_err_t err = esp_http_client_perform(http_client);
    xEventGroupClearBits(g_eg_globalsync, EVENT_OTA_HIMAX_HTTP_GOING);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "sscma ota, HTTP GET Status = %d, content_length = %"PRId64", take %lld us",
                esp_http_client_get_status_code(http_client),
                esp_http_client_get_content_length(http_client),
                esp_timer_get_time() - start);

        xSemaphoreTake(g_sem_sscma_writer_done, portMAX_DELAY);
        if (atomic_load(&g_sscma_writer_abort)) {
            // user canceled
            ret = ESP_ERR_OTA_USER_CANCELED;
        } else if (!esp_http_client_is_complete_data_received(http_client)) {
            ESP_LOGW(TAG, "sscma ota, HTTP finished but incompleted data received");
            // maybe network connection broken?
            ret = ESP_ERR_OTA_DOWNLOAD_FAIL;
        } else {
            ret = g_sscma_writer_userdata.err;  //there might be errors in sscma writer task.
        }
    } else {
        ESP_LOGE(TAG, "sscma ota, HTTP GET request failed: %s", esp_err_to_name(err));
        //error defines:
        //https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.1/esp32s3/api-reference/protocols/esp_http_client.html#macros
        ret = ESP_ERR_OTA_DOWNLOAD_FAIL;  //we sum all these errors as download failure, easier for upper caller

        atomic_store(&g_sscma_writer_abort, true);
        xSemaphoreTake(g_sem_sscma_writer_done, pdMS_TO_TICKS(10));
    }

sscma_ota_end:
    if (http_client_config) free(http_client_config);
    if (http_client) esp_http_client_cleanup(http_client);
    g_sscma_writer_userdata.http_client = NULL;

    g_result_err = ret;

    ota_status.status = g_result_err == ESP_OK ? OTA_STATUS_SUCCEED : OTA_STATUS_FAIL;
    ota_status.err_code = g_result_err;
    ota_status.percentage = g_result_err == ESP_OK ? 100 : 0;
    esp_event_post_to(app_event_loop_handle, CTRL_EVENT_BASE, ota_eventid, 
                                    &ota_status, sizeof(struct view_data_ota_status),
                                    pdMS_TO_TICKS(10000));
}

static void __app_ota_task(void *p_arg)
{
    ota_job_q_item_t *otajob = psram_calloc(1, sizeof(ota_job_q_item_t));  //never mind not releasing this
    uint32_t ota_type;

    ESP_LOGI(TAG, "starting ota task ...");

    while (1) {
        // wait for network connection
        xSemaphoreTake(g_sem_network, pdMS_TO_TICKS(10000));
        if (!atomic_load(&g_network_connected_flag))
        {
            continue;
        }

        ESP_LOGI(TAG, "network established, waiting for OTA request ...");
        //give the time to more important tasks right after the network is established
        vTaskDelay(pdMS_TO_TICKS(3000));

        do {
            if (xQueuePeek(g_Q_ota_job, otajob, portMAX_DELAY)) {
                ota_type = otajob->ota_type;

                atomic_store(&g_ota_running, true);
                if (ota_type == OTA_TYPE_ESP32) {
                    //xTaskNotifyGive(g_task_worker);  // wakeup the task
                    esp32_ota_process(otajob->url);
                } else if (ota_type == OTA_TYPE_HIMAX) {
                    sscma_ota_process(OTA_TYPE_HIMAX, otajob->url);
                } else if (ota_type == OTA_TYPE_AI_MODEL) {
                    sscma_ota_process(OTA_TYPE_AI_MODEL, otajob->url);
                    xSemaphoreGive(g_sem_ai_model_downloaded);
                } else {
                    ESP_LOGW(TAG, "unknown ota type: %" PRIu32, ota_type);
                }
                atomic_store(&g_ota_running, false);

                xQueueReceive(g_Q_ota_job, otajob, portMAX_DELAY);
            }
        } while (atomic_load(&g_network_connected_flag));
    }
}

static void ota_status_report(struct view_data_ota_status *ota_status)
{
    ESP_LOGW(TAG, "ota_status_report: status: 0x%x, err_code: 0x%x, percent: %d",
                                              ota_status->status, ota_status->err_code, ota_status->percentage);
    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_OTA_STATUS, 
                            ota_status, sizeof(struct view_data_ota_status),
                            pdMS_TO_TICKS(10000));
    // MQTT report
    const char *fmt = \
                "\"3578\": %d,"
                "\"3579\": %d,"
                "\"3580\": %d";
    const int buffsz = 1024;
    char *buff = psram_calloc(1, buffsz);
    sniprintf(buff, buffsz, fmt, ota_status->status, ota_status->err_code, ota_status->percentage);

    int len = strlen(buff);
    if (g_cur_ota_version_esp32) {
        sniprintf(buff + len, buffsz - len, ",\"3502\": \"%s\"", g_cur_ota_version_esp32);
        len = strlen(buff);
    }
    if (g_cur_ota_version_himax) {
        sniprintf(buff + len, buffsz - len, ",\"3577\": \"%s\"", g_cur_ota_version_himax);
    }

    app_sensecraft_mqtt_report_device_status_generic(buff);

    free(buff);
}

static void ota_status_report_error(esp_err_t err)
{
    struct view_data_ota_status ota_status;

    ota_status.status = SENSECRAFT_OTA_STATUS_FAIL;
    ota_status.err_code = err;
    ota_status.percentage = 0;
    
    ota_status_report(&ota_status);
}

/**
 * process the ota status, calculate a final percentage, report to mqtt broker
 * 
 * ota_event_type: [in] himax or esp32?
 * total_progress: [in] 100 or 200
 * return:
 * - ESP_OK: succeed for this ota subpart, can proceed next MCU ota if any
 * - ESP_FAIL: fail for this ota subpart, should abort the whole ota process
*/
static esp_err_t ota_status(int ota_event_type, int total_progress)
{
    esp_err_t ret = ESP_FAIL;
    ota_status_q_item_t ota_status_q_item;
    struct view_data_ota_status ota_status;
    bool be_timeout = false;
    int percentage = 0, timeout_cnt = 0;

    int timeout_max = ota_event_type == CTRL_EVENT_OTA_ESP32_FW ? (60 * 10) : (60 * 5);  //chunk of esp32 may be larger

    while (1) {
        BaseType_t res = xQueueReceive(g_Q_ota_status, &ota_status_q_item, pdMS_TO_TICKS(1000));
        if (res != pdPASS) {
            timeout_cnt++;
            if (timeout_cnt > timeout_max) {
                be_timeout = true;
                break;
            } else
                continue;
        }

        timeout_cnt = 0;
        if (ota_status_q_item.ota_src != ota_event_type) {
            ESP_LOGW(TAG, "ota status process, wrong ota src, this should not happen!!!");
            continue;
        }

        if (ota_status_q_item.ota_status.status == OTA_STATUS_DOWNLOADING) {
            int progress = ota_status_q_item.ota_status.percentage;
            if (total_progress == 200 && ota_event_type == CTRL_EVENT_OTA_ESP32_FW) {
                progress += 100;  //himax must be succeeded
            }
            percentage = (int)(100 * progress / (total_progress));
            // report
            ota_status.status = SENSECRAFT_OTA_STATUS_UPGRADING;
            ota_status.err_code = ESP_OK;
            ota_status.percentage = percentage;
            ota_status_report(&ota_status);
        }
        else if (ota_status_q_item.ota_status.status == OTA_STATUS_SUCCEED) {
            if (total_progress == 200 && ota_event_type == CTRL_EVENT_OTA_HIMAX_FW) {
                ota_status.status = SENSECRAFT_OTA_STATUS_UPGRADING;
                ota_status.percentage = 50;
            } else {
                ota_status.status = SENSECRAFT_OTA_STATUS_SUCCEED;
                ota_status.percentage = 100;
            }
            ret = ESP_OK;
            break;
        }
        else if (ota_status_q_item.ota_status.status == OTA_STATUS_FAIL) {
            break;
        }
    }

    if (be_timeout) {
        // timeout
        ota_status.status = SENSECRAFT_OTA_STATUS_FAIL;
        ota_status.err_code = ESP_ERR_OTA_TIMEOUT;
        ota_status.percentage = 0;
        ESP_LOGW(TAG, "ota status, timeout happen, it was a really long waiting (%d sec)!!!", timeout_max);
        if (ota_event_type == CTRL_EVENT_OTA_ESP32_FW) {
            xEventGroupSetBits(g_eg_globalsync, EVENT_OTA_STOP_BY_TOO_SLOW);
        } else if (ota_event_type == CTRL_EVENT_OTA_HIMAX_FW) {
            if (xEventGroupGetBits(g_eg_globalsync) & EVENT_OTA_HIMAX_HTTP_GOING) {
                if (g_sscma_writer_userdata.http_client) {
                    ESP_LOGD(TAG, "force stop the http download for himax fw");
                    esp_http_client_cancel_request(g_sscma_writer_userdata.http_client);
                    atomic_store(&g_sscma_writer_abort, true);
                }
            }
        }
    } else if (ret == ESP_OK) {
        // succeed
        ota_status.err_code = ESP_OK;
    } else {
        ota_status.status = SENSECRAFT_OTA_STATUS_FAIL;
        ota_status.err_code = ota_status_q_item.ota_status.err_code;
        ota_status.percentage = 0;
    }
    ota_status_report(&ota_status);

    return ret;
}

static void __mqtt_ota_executor_task(void *p_arg)
{
    ESP_LOGI(TAG, "starting mqtt ota executor task ...");

    while (!atomic_load(&g_network_connected_flag)) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //give the time to more important tasks right after the network is established
    //there might be queued ota requests in the `g_Q_ota_msg`, but no worry.
    vTaskDelay(pdMS_TO_TICKS(4000));

    cJSON *ota_msg_cjson;

    while (1) {
        if (xQueuePeek(g_Q_ota_msg, &ota_msg_cjson, portMAX_DELAY)) {
            if (atomic_load(&g_ota_running)) {
                ESP_LOGW(TAG, "peek a mqtt ota request, but another ota is going, can't accept 2 ota job, drop this");
                goto cleanup;  // an ota is under going, might be issued manually by console command, just drop
            }

            //lightly check validation
            bool valid = false;
            do {
                if (!cJSON_IsObject(ota_msg_cjson)) break;

                cJSON *intent = cJSONUtils_GetPointer(ota_msg_cjson, "/intent");
                if (!intent || !cJSON_IsString(intent) || strcmp(intent->valuestring, "order") != 0) break;

                cJSON *order = cJSONUtils_GetPointer(ota_msg_cjson, "/order");
                if (!order || !cJSON_IsArray(order) || cJSON_GetArraySize(order) == 0 || cJSON_GetArraySize(order) > 2) break;

                cJSON *order_name = cJSONUtils_GetPointer(ota_msg_cjson, "/order/0/name");
                if (!order_name || !cJSON_IsString(order_name) || strcmp(order_name->valuestring, "version-notify") != 0) break;

                valid = true;
            } while (0);

            if (!valid) {
                ESP_LOGW(TAG, "incoming ota cjson invalid!");
                ota_status_report_error(ESP_ERR_OTA_JSON_INVALID);
                goto cleanup;
            }

            //parse the json order
            cJSON *orders = cJSONUtils_GetPointer(ota_msg_cjson, "/order");
            int num_orders = cJSON_GetArraySize(orders);
            int total_progress = 0;

            //search himax and esp32
            int found = 0;
            cJSON *order_value_himax = NULL, *order_value_esp32 = NULL;
            g_cur_ota_version_esp32 = NULL;
            g_cur_ota_version_himax = NULL;
            bool new_himax = false, new_esp32 = false;
            cJSON *one_order;
            cJSON_ArrayForEach(one_order, orders)
            {
                cJSON *order_value = cJSON_GetObjectItem(one_order, "value");
                cJSON *order_value_sku = cJSON_GetObjectItem(order_value, "sku");
                if (order_value_sku && cJSON_IsString(order_value_sku)) {
                    if (strstr(order_value_sku->valuestring, "himax")) {
                        found++;
                        order_value_himax = order_value;
                        cJSON *fwv = cJSON_GetObjectItem(order_value_himax, "fwv");
                        if (fwv && cJSON_IsString(fwv)) {
                            //version compare
                            char *himax_version = tf_module_ai_camera_himax_version_get();
                            if (himax_version) {
                                int res = cmp_versions(fwv->valuestring, himax_version);
                                if (res <= 0) {
                                    ESP_LOGW(TAG, "himax version too old (%s <= %s), skip ...", fwv->valuestring, himax_version);
                                } else {
                                    ESP_LOGI(TAG, "will upgrade himax (%s > %s)", fwv->valuestring, himax_version);
                                    g_cur_ota_version_himax = fwv->valuestring;
                                    new_himax = true;
                                    total_progress += 100;
                                }
                            } else {
                                ESP_LOGW(TAG, "can not get himax version! Always do OTA.");
                                g_cur_ota_version_himax = fwv->valuestring;
                                new_himax = true;
                                total_progress += 100;
                            }
                        } else {
                            ESP_LOGW(TAG, "incoming ota cjson invalid, no fwv field!");
                            ota_status_report_error(ESP_ERR_OTA_JSON_INVALID);
                            goto cleanup;
                        }
                    }
                    else if (strstr(order_value_sku->valuestring, "esp32")) {
                        found++;
                        order_value_esp32 = order_value;
                        cJSON *fwv = cJSON_GetObjectItem(order_value_esp32, "fwv");
                        if (fwv && cJSON_IsString(fwv)) {
                            //version compare
                            ota_worker_task_data_t worker_data;
                            worker_call(&worker_data, CMD_esp_ota_get_running_partition);
                            esp_app_desc_t *running_app_info = psram_calloc(1, sizeof(esp_app_desc_t));
                            worker_data.app_desc = running_app_info;
                            worker_call(&worker_data, CMD_esp_ota_get_partition_description);

                            if (worker_data.err == ESP_OK) {
                                ESP_LOGI(TAG, "Running firmware version: %s", running_app_info->version);
                                app_ota_any_ignore_version_check(false);
                                int res = cmp_versions(fwv->valuestring, running_app_info->version);
                                if (res <= 0) {
                                    ESP_LOGW(TAG, "esp32 version too old (%s <= %s), skip ...", fwv->valuestring, running_app_info->version);
                                } else {
                                    ESP_LOGI(TAG, "will upgrade esp32 (%s > %s)", fwv->valuestring, running_app_info->version);
                                    g_cur_ota_version_esp32 = fwv->valuestring;
                                    new_esp32 = true;
                                    total_progress += 100;
                                }
                            } else {
                                ESP_LOGW(TAG, "Failed to get running_app_info! Always do OTA [2].");
                                g_cur_ota_version_esp32 = fwv->valuestring;
                                new_esp32 = true;
                                total_progress += 100;
                            }
                            free(running_app_info);
                        } else {
                            ESP_LOGW(TAG, "incoming ota cjson invalid, no fwv field [2]!");
                            ota_status_report_error(ESP_ERR_OTA_JSON_INVALID);
                            goto cleanup;
                        }
                    }
                }
            }
            if (found != num_orders) {
                ESP_LOGW(TAG, "incoming ota cjson invalid [2]!");
                ota_status_report_error(ESP_ERR_OTA_JSON_INVALID);
                goto cleanup;
            }

            //about to issue the ota call, clean up the ota status Q which might be filled
            //by the console ota cmd
            xQueueReset(g_Q_ota_status);

            bool need_reboot = false;
            //upgrade himax
            if (order_value_himax && new_himax) {
                cJSON *file_url = cJSON_GetObjectItem(order_value_himax, "file_url");
                if (file_url && cJSON_IsString(file_url)) {
                    esp_err_t err = app_ota_himax_fw_download(file_url->valuestring);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "app_ota_himax_fw_download err: 0x%x", err);
                        ota_status_report_error(err);
                        goto cleanup;
                    } else {
                        // now it's downloading, listen to the status events
                        // this is blocking
                        esp_err_t ret = ota_status(CTRL_EVENT_OTA_HIMAX_FW, total_progress);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "himax firmware ota failed overall!!!");
                            //status already reported in ota_status();
                            goto cleanup;
                        } else {
                            ESP_LOGI(TAG, "himax firmware ota succeeded!!!");
                            need_reboot = true;
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "incoming ota cjson invalid, no file_url field!");
                    ota_status_report_error(ESP_ERR_OTA_JSON_INVALID);
                    goto cleanup;
                }
            }

            //upgrade esp32
            if (order_value_esp32 && new_esp32) {
                cJSON *file_url = cJSON_GetObjectItem(order_value_esp32, "file_url");
                if (file_url && cJSON_IsString(file_url)) {
                    app_ota_any_ignore_version_check(false);
                    esp_err_t err = app_ota_esp32_fw_download(file_url->valuestring);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "app_ota_esp32_fw_download err: 0x%x", err);
                        ota_status_report_error(err);
                        goto cleanup;
                    } else {
                        // now it's downloading, listen to the status events
                        // this is blocking
                        esp_err_t ret = ota_status(CTRL_EVENT_OTA_ESP32_FW, total_progress);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "esp32 firmware ota failed overall!!!");
                            //status already reported in ota_status();
                            goto cleanup;
                        } else {
                            ESP_LOGI(TAG, "esp32 firmware ota succeeded!!!");
                            need_reboot = true;
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "incoming ota cjson invalid, no file_url field [2]!");
                    ota_status_report_error(ESP_ERR_OTA_JSON_INVALID);
                    goto cleanup;
                }
            }

            //lucky passthrough -_-!!
            if (need_reboot) {
                ESP_LOGW(TAG, "!!! WILL REBOOT IN 3 SEC !!!");
                vTaskDelay(pdMS_TO_TICKS(3000));  // let the last mqtt msg sent
                esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_REBOOT, NULL, 0, pdMS_TO_TICKS(10000));
            }

            //the json is used up
cleanup:
            //delete the item from Q
            xQueueReceive(g_Q_ota_msg, &ota_msg_cjson, portMAX_DELAY);
            cJSON_Delete(ota_msg_cjson);
        }
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
    if (event_base == VIEW_EVENT_BASE) {
        switch (event_id) {
            //wifi connection state changed
            case VIEW_EVENT_WIFI_ST:
            {
                ESP_LOGI(TAG, "event: VIEW_EVENT_WIFI_ST");
                struct view_data_wifi_st *p_st = (struct view_data_wifi_st *)event_data;
                atomic_store(&g_network_connected_flag, p_st->is_network);
                xSemaphoreGive(g_sem_network);
                break;
            }
            default:
                break;
        }
    }
    else if (event_base == CTRL_EVENT_BASE) {
        switch (event_id) {
            case CTRL_EVENT_MQTT_OTA_JSON:
                ESP_LOGI(TAG, "event: CTRL_EVENT_MQTT_OTA_JSON");
                if (xQueueSend(g_Q_ota_msg, event_data, 0) != pdPASS) {
                    ESP_LOGW(TAG, "can not push to ota msg Q, maybe full? drop this item!");
                }
                break;
            case CTRL_EVENT_MQTT_CONNECTED:
                ESP_LOGI(TAG, "event: CTRL_EVENT_MQTT_CONNECTED");
                atomic_store(&g_mqtt_connected_flag, true);
                break;
            case CTRL_EVENT_OTA_HIMAX_FW:
            case CTRL_EVENT_OTA_ESP32_FW:
                ESP_LOGD(TAG, "event: CTRL_EVENT_OTA_%s_FW", event_id == CTRL_EVENT_OTA_HIMAX_FW ? "HIMAX" : "ESP32");
                //event_data is ptr to struct view_data_ota_status
                ota_status_q_item_t *item = psram_calloc(1, sizeof(ota_status_q_item_t));
                item->ota_src = event_id;
                memcpy(&(item->ota_status), event_data, sizeof(struct view_data_ota_status));
                ESP_LOGD(TAG, "ota_status.status=%d, .err_code=0x%x, .percentage=%d%%", item->ota_status.status, item->ota_status.err_code,
                                                                                        item->ota_status.percentage);
                if (xQueueSend(g_Q_ota_status, item, 0) != pdPASS) {
                    ESP_LOGW(TAG, "can not push to ota status Q, maybe full? drop this item!");
                }
                free(item);  //item is copied to Q
                break;
            default:
                break;
        }
    }
}

#if (CONFIG_ENABLE_TEST_ENV && CONFIG_OTA_TEST)
static void __ota_test_timer_cb(void *arg)
{
    app_ota_ai_model_download_abort();
}

static void __ota_test_task(void *p_arg)
{
    while (!atomic_load(&g_network_connected_flag)) {
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    vTaskDelay(pdMS_TO_TICKS(5000));

    // esp_err_t res = app_ota_esp32_fw_download("https://new.pxspeed.site/factory_firmware.bin");
    // ESP_LOGI(TAG, "test app_ota_esp32_fw_download: 0x%x", res);

    esp_timer_create_args_t timer0args = {.callback = __ota_test_timer_cb};
    esp_timer_handle_t timer0;
    esp_timer_create(&timer0args, &timer0);
    esp_timer_start_once(timer0, 10*1000000);


    esp_err_t res = app_ota_ai_model_download("https://sensecraft-statics.oss-accelerate.aliyuncs.com/refer/model/1715757421743_aZ3WX5_epoch_50_int8.tflite", 0);
    ESP_LOGI(TAG, "test app_ota_ai_model_download: 0x%x", res);

    vTaskDelay(pdMS_TO_TICKS(30000));

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
    g_sem_worker_done = xSemaphoreCreateBinary();
    g_sem_sscma_writer_done = xSemaphoreCreateBinary();

    g_eg_globalsync = xEventGroupCreate();

    memset(&g_sscma_writer_userdata, 0, sizeof(ota_sscma_writer_userdata_t));

    // Q init
    const int q_size = 2;
    StaticQueue_t *q_buf = heap_caps_calloc(1, sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL);
    uint8_t *q_storage = psram_calloc(1, q_size * sizeof(void *));
    g_Q_ota_msg = xQueueCreateStatic(q_size, sizeof(void *), q_storage, q_buf);

    const int q_size1 = 10;
    StaticQueue_t *q_buf1 = heap_caps_calloc(1, sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL);
    uint8_t *q_storage1 = psram_calloc(1, q_size1 * sizeof(ota_status_q_item_t));
    g_Q_ota_status = xQueueCreateStatic(q_size1, sizeof(ota_status_q_item_t), q_storage1, q_buf1);

    const int q_size2 = 1;
    StaticQueue_t *q_buf2 = heap_caps_calloc(1, sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL);
    uint8_t *q_storage2 = psram_calloc(1, q_size2 * sizeof(ota_job_q_item_t));
    g_Q_ota_job = xQueueCreateStatic(q_size2, sizeof(ota_job_q_item_t), q_storage2, q_buf2);

    // Ringbuffer init
    StaticRingbuffer_t *buffer_struct = (StaticRingbuffer_t *)psram_calloc(1, sizeof(StaticRingbuffer_t));
    uint8_t *buffer_storage = (uint8_t *)psram_calloc(1, AI_MODEL_RINGBUFF_SIZE);
    g_rb_ai_model = xRingbufferCreateStatic(AI_MODEL_RINGBUFF_SIZE, RINGBUF_TYPE_BYTEBUF, buffer_storage, buffer_struct);

    // ota main task
    const uint32_t stack_size = 10 * 1024;
    StackType_t *task_stack = (StackType_t *)psram_calloc(1, stack_size * sizeof(StackType_t));
    g_task = xTaskCreateStatic(__app_ota_task, "app_ota", stack_size, NULL, 9, task_stack, &g_task_tcb);

    // task for handling incoming mqtt
    StackType_t *task_stack1 = (StackType_t *)psram_calloc(1, stack_size * sizeof(StackType_t));
    StaticTask_t *task_tcb1 = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    xTaskCreateStatic(__mqtt_ota_executor_task, "mqtt_ota", stack_size, NULL, 2, task_stack1, task_tcb1);

    // task for sscma writer
    StackType_t *task_stack2 = (StackType_t *)psram_calloc(1, stack_size * sizeof(StackType_t));
    StaticTask_t *task_tcb2 = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    g_task_sscma_writer = xTaskCreateStatic(__sscma_writer_task, "ota_sscma_writer", stack_size, NULL, 9, task_stack2, task_tcb2);

    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, __sys_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTP_CLIENT_EVENT, ESP_EVENT_ANY_ID, __sys_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST,
                                                    __app_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_MQTT_CONNECTED,
                                                    __app_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_MQTT_OTA_JSON,
                                                    __app_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(app_event_loop_handle, OTA_EVENT_BASE, ESP_EVENT_ANY_ID,
                                                    __ota_event_handler, NULL));
    // ota status handling
    ESP_ERROR_CHECK(esp_event_handler_register_with(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_OTA_HIMAX_FW,
                                                    __app_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(app_event_loop_handle, CTRL_EVENT_BASE, CTRL_EVENT_OTA_ESP32_FW,
                                                    __app_event_handler, NULL));

#if (CONFIG_ENABLE_TEST_ENV && CONFIG_OTA_TEST)
    StackType_t *task_stacktest = (StackType_t *)psram_calloc(1, stack_size * sizeof(StackType_t));
    StaticTask_t *task_tcbtest = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    xTaskCreateStatic(__ota_test_task, "ota_test", stack_size, NULL, 1, task_stacktest, task_tcbtest);
#endif

    return ESP_OK;
}

esp_err_t app_ota_ai_model_download(char *url, int size_bytes)
{
    xEventGroupSetBits(g_eg_globalsync, EVENT_AI_MODEL_DL_PREPARING);
    //check network connection first
    if (!atomic_load(&g_network_connected_flag)) {
        //just booted or network reconnecting, ensure timing for all tasks depending on network
        int i;
        for (i = 0; i < 30; i++) {
            if (atomic_load(&g_network_connected_flag)) break;
            if (xEventGroupGetBits(g_eg_globalsync) & EVENT_AI_MODEL_DL_EARLY_ABORT)
                return ESP_ERR_OTA_USER_CANCELED;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (i == 30) return ESP_ERR_OTA_CONNECTION_FAIL;
        
        for (i = 0; i < 10; i++) {
            if (xEventGroupGetBits(g_eg_globalsync) & EVENT_AI_MODEL_DL_EARLY_ABORT)
                return ESP_ERR_OTA_USER_CANCELED;
            if (atomic_load(&g_mqtt_connected_flag)) {
                vTaskDelay(pdMS_TO_TICKS(2000));  //schedule this behind FW ota from MQTT
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        //we'll start ai model download anyway, regardless of mqtt connection
    }
    if (xEventGroupGetBits(g_eg_globalsync) & EVENT_AI_MODEL_DL_EARLY_ABORT)
        return ESP_ERR_OTA_USER_CANCELED;

    ota_job_q_item_t *otajob = psram_calloc(1, sizeof(ota_job_q_item_t));
    otajob->ota_type = OTA_TYPE_AI_MODEL;
    memcpy(otajob->url, url, strlen(url));
    otajob->file_size = size_bytes;

    if (xQueueSend(g_Q_ota_job, otajob, 0) != pdPASS) {
        free(otajob);
        return ESP_ERR_OTA_ALREADY_RUNNING;
    }

    free(otajob);

    // block until ai model download completed or failed
    xSemaphoreTake(g_sem_ai_model_downloaded, portMAX_DELAY);

    return g_result_err;
}

esp_err_t app_ota_ai_model_download_abort()
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "app_ota_ai_model_download_abort ...");
    if (xEventGroupGetBits(g_eg_globalsync) & EVENT_AI_MODEL_DL_PREPARING) {
        xEventGroupClearBits(g_eg_globalsync, EVENT_AI_MODEL_DL_PREPARING);
        xEventGroupSetBits(g_eg_globalsync, EVENT_AI_MODEL_DL_EARLY_ABORT);
        ESP_LOGD(TAG, "app_ota_ai_model_download_abort aborted in early phase");
        vTaskDelay(pdMS_TO_TICKS(1000));
        return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(1));  //there's a small gap between EVENT_AI_MODEL_DL_PREPARING and EVENT_OTA_HIMAX_HTTP_GOING
    if ((xEventGroupGetBits(g_eg_globalsync) & EVENT_OTA_HIMAX_HTTP_GOING) && 
        g_sscma_writer_userdata.http_client && g_sscma_writer_userdata.ota_type == OTA_TYPE_AI_MODEL)
    {
        ret = esp_http_client_cancel_request(g_sscma_writer_userdata.http_client);
        atomic_store(&g_sscma_writer_abort, true);
        ESP_LOGD(TAG, "app_ota_ai_model_download_abort aborted the http download");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return ret;
}

esp_err_t app_ota_esp32_fw_download(char *url)
{
    esp_err_t ret = ESP_OK;

    ota_job_q_item_t *otajob = psram_calloc(1, sizeof(ota_job_q_item_t));
    otajob->ota_type = OTA_TYPE_ESP32;
    memcpy(otajob->url, url, strlen(url));

    if (xQueueSend(g_Q_ota_job, otajob, 0) != pdPASS) ret = ESP_ERR_OTA_ALREADY_RUNNING;

    //the ota_job_q_item_t is copied into Q, now can be released
    free(otajob);

    return ret;
}

esp_err_t app_ota_himax_fw_download(char *url)
{
    esp_err_t ret = ESP_OK;

    ota_job_q_item_t *otajob = psram_calloc(1, sizeof(ota_job_q_item_t));
    otajob->ota_type = OTA_TYPE_HIMAX;
    memcpy(otajob->url, url, strlen(url));

    if (xQueueSend(g_Q_ota_job, otajob, 0) != pdPASS) ret = ESP_ERR_OTA_ALREADY_RUNNING;

    //the ota_job_q_item_t is copied into Q, now can be released
    free(otajob);

    return ret;
}

void  app_ota_any_ignore_version_check(bool ignore)
{
    atomic_store(&g_ignore_version_check, ignore);
}
