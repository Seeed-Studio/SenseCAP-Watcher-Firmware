#include "app_png.h"
#include "esp_log.h"
#include "data_defs.h"
#include "event_loops.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "storage.h"
#include "app_png.h"
#include "util/util.h"

#define TAG              "HTTP_EMOJI"
#define HTTP_MOUNT_POINT "/spiffs"

#define EMOJI_HTTP_TIMEOUT_MS           30000
#define EMOJI_HTTP_DOWNLOAD_RETRY_TIMES 5
#define EMOJI_HTTP_RX_CHUNK_SIZE        512




#define MAX_RETRY_COUNT      5
#define HTTP_MAX_BUFFER_SIZE (100 * 1024)
#define EMOJI_HTTP_TIMEOUT_MS 5000
#define HTTP_MOUNT_POINT "/spiffs"

typedef struct {
    esp_http_client_config_t config;
    FILE *f;
    char *file_path;
    int64_t file_start_time;
    bool download_complete;
    int64_t content_length;
    char *buffer;
    int buffer_size;
} download_task_arg_t;


static EventGroupHandle_t download_event_group;
static const int DOWNLOAD_COMPLETE_BIT = BIT0;
static SemaphoreHandle_t download_mutex;
static int64_t total_data_size = 0;


// // define global image data store and count variable
lv_img_dsc_t *g_detect_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_speak_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_listen_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_anaylze_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_standby_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_greet_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_detected_img_dsc[MAX_IMAGES];

int g_detect_image_count = 0;
int g_speak_image_count = 0;
int g_listen_image_count = 0;
int g_analyze_image_count = 0;
int g_standby_image_count = 0;
int g_greet_image_count = 0;
int g_detected_image_count = 0;

void create_img_dsc(lv_img_dsc_t **img_dsc, void *data, size_t size) {
    *img_dsc = (lv_img_dsc_t *)heap_caps_malloc(sizeof(lv_img_dsc_t), MALLOC_CAP_SPIRAM);

    if (*img_dsc == NULL) {
        ESP_LOGE("Image DSC", "Failed to allocate memory for image descriptor");
        return;
    }

    (*img_dsc)->header.always_zero = 0;
    (*img_dsc)->header.w = 412;
    (*img_dsc)->header.h = 412;
    (*img_dsc)->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    (*img_dsc)->data_size = size;
    (*img_dsc)->data = data;
}

// Function to read and store PNG files into PSRAM
void* read_png_to_psram(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        ESP_LOGE("SPIFFS", "Failed to open file: %s", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *png_buffer = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!png_buffer) {
        ESP_LOGE("PSRAM", "Failed to allocate PSRAM for image buffer");
        fclose(file);
        return NULL;
    }

    fread(png_buffer, 1, file_size, file);
    fclose(file);

    *out_size = file_size;
    return png_buffer;
}

// Function to create a black image buffer
void* create_black_image(size_t size) {
    void *black_buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!black_buffer) {
        ESP_LOGE("PSRAM", "Failed to allocate PSRAM for black image buffer");
        return NULL;
    }
    memset(black_buffer, 0, size);
    return black_buffer;
}

// Helper function to check if the file name matches the specified prefix and is a PNG file
static int is_png_file_for_expression(const char* filename, const char* prefix) {
    const char *suffix = ".png";
    size_t len = strlen(filename);
    size_t suffix_len = strlen(suffix);
    if (len > suffix_len && strcmp(filename + len - suffix_len, suffix) == 0) {
        if (strncmp(filename, prefix, strlen(prefix)) == 0) {
            return 1;
        }
    }
    return 0;
}

// Function to read and store selected PNG files based on prefix
void read_and_store_selected_pngs(const char *file_prefix, lv_img_dsc_t **img_dsc_array, int *image_count) {
    DIR *dir;
    struct dirent *ent;
    bool image_loaded = false;
    if ((dir = opendir("/spiffs")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (is_png_file_for_expression(ent->d_name, file_prefix)) {
                if (*image_count >= MAX_IMAGES) {
                    ESP_LOGW("PNG Load", "Maximum image storage reached, cannot load more images");
                    break;
                }
                
                size_t size;
                char filepath[256];
                sprintf(filepath, "/spiffs/%s", ent->d_name);
                void *data = read_png_to_psram(filepath, &size);
                if (data) {
                    ESP_LOGI("PNG Load", "Loaded %s into PSRAM", ent->d_name);
                    
                    create_img_dsc(&img_dsc_array[*image_count], data, size);
                    (*image_count)++;
                    image_loaded = true;
                    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_PNG_LOADING, NULL, NULL, pdMS_TO_TICKS(10000));
                }
            }
        }
        closedir(dir);
    }
    else
    {
        ESP_LOGE("SPIFFS", "Failed to open directory /spiffs");
    }
}




static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    download_task_arg_t *task_arg = (download_task_arg_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            task_arg->file_start_time = esp_timer_get_time();
            task_arg->buffer_size = 0;
            task_arg->buffer = heap_caps_malloc(HTTP_MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
            if (task_arg->buffer == NULL) {
                ESP_LOGE(TAG, "Failed to allocate PSRAM buffer");
                return ESP_FAIL;
            }
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                if (task_arg->buffer_size + evt->data_len <= HTTP_MAX_BUFFER_SIZE) {
                    memcpy(task_arg->buffer + task_arg->buffer_size, evt->data, evt->data_len);
                    task_arg->buffer_size += evt->data_len;
                } else {
                    ESP_LOGE(TAG, "PSRAM buffer overflow");
                    return ESP_FAIL;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            task_arg->content_length = esp_http_client_get_content_length(evt->client);
            task_arg->download_complete = true;
            esp_err_t err = storage_file_write(task_arg->file_path, task_arg->buffer, task_arg->buffer_size);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write data to file using storage_file_write");
                return err;
            }
            heap_caps_free(task_arg->buffer);
            task_arg->buffer = NULL;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (task_arg->buffer != NULL) {
                heap_caps_free(task_arg->buffer);
                task_arg->buffer = NULL;
            }
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}


static void download_task(void *arg) {
    download_task_arg_t *task_arg = (download_task_arg_t *)arg;
    esp_http_client_handle_t client = esp_http_client_init(&task_arg->config);
    task_arg->download_complete = false;
    esp_err_t final_err = ESP_OK;

    for (int i = 0; i < MAX_RETRY_COUNT; i++) {
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int64_t download_time = esp_timer_get_time() - task_arg->file_start_time;
            ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld, download time = %lld us",
                     esp_http_client_get_status_code(client), task_arg->content_length, download_time);
            xSemaphoreTake(download_mutex, portMAX_DELAY);
            total_data_size += task_arg->content_length;
            xSemaphoreGive(download_mutex);
            break;
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed: %s. Retrying...", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            vTaskDelay(500 / portTICK_PERIOD_MS); // Wait for a second before retrying
            client = esp_http_client_init(&task_arg->config);
            final_err = err;
        }
    }

    while (!task_arg->download_complete) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    esp_http_client_cleanup(client);
    free(task_arg->file_path);
    free(task_arg);

    xEventGroupSetBits(download_event_group, DOWNLOAD_COMPLETE_BIT);
    vTaskDelete(NULL);
}

download_summary_t download_emoji_images(char *base_name, char *urls[], int url_count) {
    ESP_LOGI(TAG, "Starting emoji HTTP download, base file name = %s ...", base_name);
    int64_t total_start_time = esp_timer_get_time();
    download_event_group = xEventGroupCreate();
    download_mutex = xSemaphoreCreateMutex();
    EventBits_t bits;
    download_result_t *results = calloc(url_count, sizeof(download_result_t));

    for (int url_index = 0; url_index < url_count; url_index++) {
        char name_with_index[50];
        snprintf(name_with_index, sizeof(name_with_index), "%s%d", base_name, url_index);
        char *emoji_name = strdup(name_with_index);

        download_task_arg_t *task_arg = (download_task_arg_t *)calloc(1, sizeof(download_task_arg_t));
        task_arg->config.url = urls[url_index];
        task_arg->config.method = HTTP_METHOD_GET;
        task_arg->config.timeout_ms = EMOJI_HTTP_TIMEOUT_MS;
        task_arg->config.crt_bundle_attach = esp_crt_bundle_attach;
        task_arg->config.buffer_size = HTTP_MAX_BUFFER_SIZE;
        task_arg->config.event_handler = _http_event_handler;
        task_arg->config.user_data = task_arg;

        asprintf(&task_arg->file_path, "%s/%s.png", HTTP_MOUNT_POINT, emoji_name);

        StackType_t *task_stack = (StackType_t *)heap_caps_malloc(8192 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
        StaticTask_t *task_buffer = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);

        if (task_stack == NULL || task_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for task stack or control block");
            free(task_stack);
            free(task_buffer);
            free(task_arg->file_path);
            free(task_arg);
            results[url_index].success = false;
            results[url_index].error_code = ESP_ERR_NO_MEM;
            free(emoji_name);
            continue;
        }

        xTaskCreateStatic(download_task, "download_task", 8192, task_arg, 9, task_stack, task_buffer);

        free(emoji_name);
    }

    for (int i = 0; i < url_count; i++) {
        bits = xEventGroupWaitBits(download_event_group, DOWNLOAD_COMPLETE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & DOWNLOAD_COMPLETE_BIT) {
            ESP_LOGI(TAG, "Download task %d completed", i);
            results[i].success = true;
            results[i].error_code = ESP_OK;
        } else {
            results[i].success = false;
            results[i].error_code = ESP_FAIL;

    if (!image_loaded && *image_count < MAX_IMAGES) {
        ESP_LOGW("PNG Load", "No image found for prefix %s, creating a black image", file_prefix);
        size_t size = 412 * 412 * 3; // Assuming the size for a 412x412 image with alpha channel
        void *black_data = create_black_image(size);
        if (black_data) {
            create_img_dsc(&img_dsc_array[*image_count], black_data, size);
            (*image_count)++;

        }
    }

    int64_t total_end_time = esp_timer_get_time();
    int64_t total_time_us = total_end_time - total_start_time;
    double total_time_s = total_time_us / 1000000.0;
    double download_speed = total_data_size / total_time_s;

    ESP_LOGD(TAG, "Total download size: %" PRId64 " bytes", total_data_size);
    total_data_size = 0;
    ESP_LOGD(TAG, "Total download time: %.2f seconds", total_time_s);
    ESP_LOGD(TAG, "Overall download speed: %.2f bytes/second", download_speed);

    vEventGroupDelete(download_event_group);
    vSemaphoreDelete(download_mutex);

    download_summary_t summary = {
        .results = results,
        .total_time_us = total_time_us,
        .download_speed = download_speed
    };

    return summary;
}
