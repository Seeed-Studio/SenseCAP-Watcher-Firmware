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
#include "cJSON.h"
#include "mbedtls/md5.h"

#define TAG              "HTTP_EMOJI"

#define EMOJI_HTTP_TIMEOUT_MS           30000
#define EMOJI_HTTP_DOWNLOAD_RETRY_TIMES 5
#define EMOJI_HTTP_RX_CHUNK_SIZE        512

#define MAX_RETRY_COUNT                 5
#define HTTP_MAX_BUFFER_SIZE            (100 * 1024)
#define EMOJI_HTTP_TIMEOUT_MS           5000
#define STORAGE_MOUNT_POINT             "/spiffs"

#define ERR_EMOJI_DL_BAD_HTTP_LEN       0x200

const char manifest_json[] = \
"{  \
    \"greeting1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"27f6dcf567c8998d7613e92de956632e\",  \
        \"size\": 25299  \
    }],  \
    \"greeting2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"8129b5d053cccd27a06b1c8ff505f24f\",  \
        \"size\": 26126  \
    }],  \
    \"greeting3.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"27f6dcf567c8998d7613e92de956632e\",  \
        \"size\": 25299  \
    }],  \
    \"detecting1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"5d85490ea8b96804257df3ec733a59b9\",  \
        \"size\": 24828  \
    }],  \
    \"detecting2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"a61f45078cbfbfcb4975b86761ae3072\",  \
        \"size\": 30668  \
    }],  \
    \"detecting3.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"415a1b2e409805c20cfe496580c5989f\",  \
        \"size\": 41195  \
    }],  \
    \"detecting4.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"240a57c329f601debccd54bf77789df9\",  \
        \"size\": 30320  \
    }],  \
    \"detecting5.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"2fbb2d77a8cff9cf597f9ea5e4bf6f50\",  \
        \"size\": 26572  \
    }],  \
    \"detected1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"86985b7726fee05dd1e0fb09ce8fe1eb\",  \
        \"size\": 41702  \
    }],  \
    \"detected2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"46af667ccd7cc77aec7e10571bd48329\",  \
        \"size\": 40278  \
    }],  \
    \"speaking1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"e6aabf2716848775de7e242587c52ea3\",  \
        \"size\": 36941  \
    }],  \
    \"speaking2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"f86a568403f8ea7d343148817a05dec4\",  \
        \"size\": 8164  \
    }],  \
    \"speaking3.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"e6aabf2716848775de7e242587c52ea3\",  \
        \"size\": 36941  \
    }],  \
    \"listening1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"6d1b1d8c0538801d0ab269a149755447\",  \
        \"size\": 22670  \
    }],  \
    \"listening2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"28563ffb3e2f52e6966c3eb0c195424f\",  \
        \"size\": 21698  \
    }],  \
    \"standby1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"40206d053f88b522ecd81fbaadafd2ab\",  \
        \"size\": 25354  \
    }],  \
    \"standby2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"b1455405767e00d15b61e22c62adafd9\",  \
        \"size\": 29218  \
    }],  \
    \"standby3.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"94820b70ac5c8bb82776cee7cbce82e8\",  \
        \"size\": 34889  \
    }],  \
    \"standby4.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"41e72f42890310eb4e8597d683f63f02\",  \
        \"size\": 40102  \
    }],  \
    \"analyzing1.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"1ad6b8540f5237e2a2d65883c0900c5c\",  \
        \"size\": 31945  \
    }],  \
    \"analyzing2.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"11a0c481b4fbf481efac4500fb80b825\",  \
        \"size\": 32828  \
    }],  \
    \"analyzing3.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"6f84161057effdf479feca369c85b42f\",  \
        \"size\": 31808  \
    }],  \
    \"analyzing4.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"8a79555539d0965f40edcf5a693b67bb\",  \
        \"size\": 32057  \
    }],  \
    \"analyzing5.png\": [{  \
        \"url\": \"www.baidu.com\",  \
        \"checksum\": \"56a438f2aa1c4c53f13d3ad7eca2ba45\",  \
        \"size\": 32889  \
    }]  \
}";


typedef struct {
    StackType_t *stack;
    StaticTask_t *tcb;
    int task_num;
    esp_http_client_config_t config;
    FILE *f;
    char file_path[256];
    int64_t file_start_time;
    bool download_complete;
    int64_t content_length;
    char *buffer;
    int buffer_size;
    esp_err_t err;
    esp_err_t http_err;
} download_task_arg_t;


static EventGroupHandle_t download_event_group;
static SemaphoreHandle_t download_mutex;
static int64_t total_data_size = 0;


// Function declarations
void create_black_image_and_store(lv_img_dsc_t **img_dsc_array, int *image_count, size_t size);
bool validate_image(const char *name, size_t size, const char *checksum, cJSON *manifest);

lv_img_dsc_t *g_detect_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_speak_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_listen_img_dsc[MAX_IMAGES];
lv_img_dsc_t *g_analyze_img_dsc[MAX_IMAGES];
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

char* read_json_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        ESP_LOGE("SPIFFS", "Failed to open JSON file: %s", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(length + 1);
    if (!content) {
        ESP_LOGE("SPIFFS", "Failed to allocate memory for JSON content");
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';

    fclose(file);
    return content;
}

bool validate_image(const char *name, size_t size, const char *checksum, cJSON *manifest) {
    cJSON *file_array = cJSON_GetObjectItemCaseSensitive(manifest, name);
    if (!file_array || !cJSON_IsArray(file_array)) return false;

    cJSON *file = cJSON_GetArrayItem(file_array, 0);
    if (!file) return false;

    cJSON *file_size = cJSON_GetObjectItemCaseSensitive(file, "size");
    cJSON *file_checksum = cJSON_GetObjectItemCaseSensitive(file, "checksum");

    if (cJSON_IsNumber(file_size) && file_size->valueint == size &&
        cJSON_IsString(file_checksum) && strcmp(file_checksum->valuestring, checksum) == 0) {
        return true;
    }

    return false;
}

char* calculate_file_md5(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *file_buffer = psram_malloc(file_size);
    if (!file_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for file buffer");
        fclose(f);
        return NULL;
    }

    fread(file_buffer, 1, file_size, f);
    fclose(f);

    unsigned char md5_checksum[16];
    mbedtls_md5(file_buffer, file_size, md5_checksum);

    char *checksum_hex = psram_malloc(33);
    if (!checksum_hex) {
        ESP_LOGE(TAG, "Failed to allocate memory for checksum_hex");
        free(file_buffer);
        return NULL;
    }

    for (int i = 0; i < 16; i++) {
        sprintf(checksum_hex + (i * 2), "%02x", md5_checksum[i]);
    }
    checksum_hex[32] = '\0';

    free(file_buffer);
    return checksum_hex;
}

void check_and_download_files() {
    cJSON *manifest = cJSON_Parse(manifest_json);
    if (!manifest) {
        ESP_LOGE(TAG, "Failed to parse manifest.json");
        return;
    }

    cJSON * download_list = cJSON_CreateArray();
    if(!download_list)
    {
        ESP_LOGE(TAG, "Failed to create download list");
        cJSON_Delete(manifest);
        return;
    }

    cJSON *file;
    cJSON_ArrayForEach(file, manifest) {
        const char *file_name = file->string;
        cJSON *file_array = cJSON_GetObjectItem(manifest, file_name);
        cJSON *file_obj = cJSON_GetArrayItem(file_array, 0);
        cJSON *url_item = cJSON_GetObjectItem(file_obj, "url");
        cJSON *size_item = cJSON_GetObjectItem(file_obj, "size");
        cJSON *checksum_item = cJSON_GetObjectItem(file_obj, "checksum");

        if (!url_item || !size_item || !checksum_item) {
            ESP_LOGE(TAG, "Invalid manifest entry for %s", file_name);
            continue;
        }

        const char *url = url_item->valuestring;
        size_t size = size_item->valueint;
        const char *checksum = checksum_item->valuestring;

        // ESP_LOGI(TAG, "File: %s, URL: %s, Size: %zu", file_name, url, size);

        char file_path[256];
        sprintf(file_path, "/spiffs/%s", file_name);

        FILE *f = fopen(file_path, "rb");
        if (!f) {
            ESP_LOGI(TAG, "File %s not found, requesting download", file_name);
            cJSON *download_item = cJSON_CreateObject();
            cJSON_AddStringToObject(download_item, "name", file_name);
            cJSON_AddStringToObject(download_item, "url", url);
            cJSON_AddStringToObject(download_item, "checksum", checksum);
            cJSON_AddStringToObject(download_item, "size", size);
            cJSON_AddItemToArray(download_list, download_item);
            continue;
        }

        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *file_checksum = calculate_file_md5(file_path);
        fclose(f);

        if (!validate_image(file_name, file_size, file_checksum, manifest)) {
            ESP_LOGW(TAG, "File %s validation failed, checksum is %s, requesting download", file_name, file_checksum);
            cJSON *download_item = cJSON_CreateObject();
            cJSON_AddStringToObject(download_item, "name", file_name);
            cJSON_AddStringToObject(download_item, "url", url);
            cJSON_AddStringToObject(download_item, "checksum", checksum);
            cJSON_AddStringToObject(download_item, "size", size);
            cJSON_AddItemToArray(download_list, download_item);
        } else {
            ESP_LOGI(TAG, "File %s validated successfully, checksum is %s", file_name, file_checksum);
        }

        free(file_checksum);
    }

    if(cJSON_GetArraySize(download_list) > 0)
    {
        char * download_list_string = cJSON_Print(download_list);
        if(download_list_string)
        {
            ESP_LOGI(TAG, "Download list: %s", download_list_string);
            // TODO: Send the download list to the download request function
            // send_download_request("download_list", download_list_string);
            free(download_list_string);
        }
    }else{
        ESP_LOGI(TAG, "All files validated successfully, no downloads needed");
    }

    cJSON_Delete(download_list);
    cJSON_Delete(manifest);
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

    // ESP_LOGI("Image Size", "Loaded %s, size: %.2f KB", path, file_size / 1024.0);

    return png_buffer;
}

// Function to create a black image buffer with a label
void* create_black_image_with_label(size_t size, const char *label) {
    void *black_buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!black_buffer) {
        ESP_LOGE("PSRAM", "Failed to allocate PSRAM for black image buffer");
        return NULL;
    }
    memset(black_buffer, 0, size);

    // Add label to the black buffer
    // This could be done using a graphics library to draw the label text onto the black buffer
    // For simplicity, we are just logging it here
    ESP_LOGW("Black Image", "Created black image with label: %s", label);

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

// Helper function to load images based on prefix
bool load_images(const char *prefix, lv_img_dsc_t **img_dsc_array, int *image_count) {
    DIR *dir;
    struct dirent *ent;
    bool loaded = false;

    if ((dir = opendir("/spiffs")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (is_png_file_for_expression(ent->d_name, prefix)) {
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
                    loaded = true;
                    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_PNG_LOADING, NULL, NULL, pdMS_TO_TICKS(10000));
                }
            }
        }
        closedir(dir);
    } else {
        ESP_LOGE("SPIFFS", "Failed to open directory /spiffs");
    }

    return loaded;
}

// Function to read and store selected PNG files based on prefix
void read_and_store_selected_pngs(const char *primary_prefix, const char *secondary_prefix, lv_img_dsc_t **img_dsc_array, int *image_count) {
    bool image_loaded = false;

    // Try loading images with primary prefix
    image_loaded = load_images(primary_prefix, img_dsc_array, image_count);

    // If no images loaded with primary prefix, try secondary prefix
    if (!image_loaded) {
        ESP_LOGW("PNG Load", "No images found with primary prefix %s, trying secondary prefix %s", primary_prefix, secondary_prefix);
        image_loaded = load_images(secondary_prefix, img_dsc_array, image_count);
    }

    // If no images loaded with either prefix, create black images with labels
    if (!image_loaded && *image_count < MAX_IMAGES) {
        ESP_LOGW("PNG Load", "No images found with either prefix, creating black images with labels");
        size_t size = 412 * 412 * 3; // Assuming the size for a 412x412 image with alpha channel
        void *black_data = create_black_image_with_label(size, primary_prefix);
        if (black_data) {
            create_img_dsc(&img_dsc_array[*image_count], black_data, size);
            (*image_count)++;
        }
    }
}



static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    download_task_arg_t *task_arg = (download_task_arg_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_ON_CONNECTED");
            task_arg->file_start_time = esp_timer_get_time();
            task_arg->content_length = 0;
            task_arg->buffer_size = 0;
            if (task_arg->buffer) {
                free(task_arg->buffer);
                task_arg->buffer = NULL;
            }
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (task_arg->content_length == 0) {
                task_arg->content_length = esp_http_client_get_content_length(evt->client);
                ESP_LOGI(TAG, "downloader, HTTP_EVENT_ON_DATA, content_len=%lld", task_arg->content_length);
                if (task_arg->content_length == 0) {
                    task_arg->http_err = ERR_EMOJI_DL_BAD_HTTP_LEN;
                    break;
                }
                if (task_arg->buffer == NULL)
                    task_arg->buffer = psram_calloc(1, task_arg->content_length);

                if (task_arg->buffer == NULL) {
                    task_arg->http_err = ESP_ERR_NO_MEM;
                    break;
                }
                task_arg->buffer_size = 0;
            }
            if (evt->data_len > 0) {
                if (task_arg->buffer_size + evt->data_len <= task_arg->content_length) {
                    memcpy(task_arg->buffer + task_arg->buffer_size, evt->data, evt->data_len);
                    task_arg->buffer_size += evt->data_len;
                } else {
                    ESP_LOGE(TAG, "downloader, data can not fit in content buffer, this should not happen!!!");
                    task_arg->http_err = ESP_ERR_NO_MEM;
                    break;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "downloader, HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

static void download_task(void *arg) {
    download_task_arg_t *task_arg = (download_task_arg_t *)arg;
    esp_http_client_handle_t client = esp_http_client_init(&task_arg->config);
    task_arg->download_complete = false;
    task_arg->err = ESP_OK;
    task_arg->http_err = ESP_OK;

    for (int i = 0; i < MAX_RETRY_COUNT; i++) {
        esp_err_t err = esp_http_client_perform(client);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            int64_t download_time = esp_timer_get_time() - task_arg->file_start_time;
            ESP_LOGI(TAG, "downloader, HTTP GET Status = %d, content_length = %lld, download time = %lld us",
                     esp_http_client_get_status_code(client), task_arg->content_length, download_time);
            if (!esp_http_client_is_complete_data_received(client)) {
                ESP_LOGW(TAG, "downloader, HTTP finished but incompleted data received");
                task_arg->err = task_arg->http_err != ESP_OK ? task_arg->http_err : ESP_ERR_NOT_FINISHED;
                continue;
            }
            xSemaphoreTake(download_mutex, portMAX_DELAY);
            total_data_size += task_arg->content_length;
            task_arg->download_complete = true;
            task_arg->err = ESP_OK;
            xSemaphoreGive(download_mutex);
            break;
        } else {
            ESP_LOGE(TAG, "downloader, HTTP GET request failed: %s. Retrying...", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            vTaskDelay(500 / portTICK_PERIOD_MS); // Wait for a second before retrying
            client = esp_http_client_init(&task_arg->config);
            task_arg->err = err;
        }
    }

    if (!task_arg->download_complete) {
        goto download_task_end;
    }

    esp_err_t err2 = storage_file_write(task_arg->file_path, task_arg->buffer, task_arg->buffer_size);
    if (err2 != ESP_OK) {
        ESP_LOGE(TAG, "downloader, failed to write data to file using storage_file_write");
        task_arg->err = err2;
    } else {
        ESP_LOGI(TAG, "downloader, saved file %s", task_arg->file_path);
    }

download_task_end:
    esp_http_client_cleanup(client);
    if (task_arg->buffer) {
        free(task_arg->buffer);
        task_arg->buffer = NULL;
    }

    xEventGroupSetBits(download_event_group, BIT0 << (task_arg->task_num));
    vTaskDelete(NULL);
}

esp_err_t download_emoji_images(download_summary_t *summary, cJSON *filename, cJSON *url_array, int url_count)
{
    char *base_name = filename->valuestring;
    ESP_LOGI(TAG, "Starting emoji HTTP download, base file name = %s ...", filename->valuestring);

    int64_t total_start_time = esp_timer_get_time();
    download_event_group = xEventGroupCreate();
    download_mutex = xSemaphoreCreateMutex();
    EventBits_t bits, bits_all, bit_mask = 0;
    download_task_arg_t *task_args = (download_task_arg_t *)psram_calloc(url_count, sizeof(download_task_arg_t));
    esp_err_t ret = ESP_OK;

    for (int url_index = 0; url_index < url_count; url_index++) {
        cJSON *url_item = cJSON_GetArrayItem(url_array, url_index);

        download_task_arg_t *task_arg = &task_args[url_index];
        task_arg->task_num = url_index;

        task_arg->config.url = url_item->valuestring;
        task_arg->config.method = HTTP_METHOD_GET;
        task_arg->config.timeout_ms = EMOJI_HTTP_TIMEOUT_MS;
        task_arg->config.crt_bundle_attach = esp_crt_bundle_attach;
        task_arg->config.buffer_size = HTTP_MAX_BUFFER_SIZE;
        task_arg->config.event_handler = _http_event_handler;
        task_arg->config.user_data = task_arg;

        sniprintf(task_arg->file_path, 255/*leave the last zero*/, "%s/Custom_%s%d.png", STORAGE_MOUNT_POINT, base_name, url_index+1);

        const int stack_size = 8192;
        StackType_t *task_stack = (StackType_t *)psram_calloc(stack_size, sizeof(StackType_t));
        StaticTask_t *task_buffer = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);

        task_arg->stack = task_stack;
        task_arg->tcb = task_buffer;

        if (task_stack == NULL || task_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for task stack or control block");
            ret = ESP_ERR_NO_MEM;
            goto download_emoji_cleanup;
        }

        ESP_LOGI(TAG, "create downloader task to download %s, save to %s", url_item->valuestring, task_arg->file_path);

        xTaskCreateStatic(download_task, "download_task", stack_size, task_arg, 9, task_stack, task_buffer);

        bit_mask |= (BIT0 << url_index);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    int i;
    int emoticon_download_per;
    bits_all = 0x0;
    for (i = 0 ; i < MAX_IMAGES; i++) {
        bits = xEventGroupWaitBits(download_event_group, bit_mask, pdFALSE, pdFALSE, pdMS_TO_TICKS(60000));
        bits_all |= bits;
        bit_mask &= ~bits;  //don't wait on these bits next time
        int cnt = 0, succ_cnt = 0;
        for (int j = 0; j < url_count; j++)
        {
            download_task_arg_t *task_arg = &task_args[j];
            if (bits_all & (BIT0 << j)) {
                cnt++;
                if (task_arg->err == ESP_OK) succ_cnt++;
            }
        }
        ESP_LOGI(TAG, "emoji download progress, %d downloader task joined (bits: 0x%x), succ_cnt: %d", cnt, bits_all, succ_cnt);
        emoticon_download_per = succ_cnt * 100 / url_count;
        if (emoticon_download_per > 0) {
            esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_EMOJI_DOWLOAD_BAR, 
                              &emoticon_download_per, sizeof(int), pdMS_TO_TICKS(10000));
        }
        if (cnt == url_count) break;
    }

    if (i == MAX_IMAGES) {
        ESP_LOGE(TAG, "emoji download failed, timeout waiting for all tasks join");
        ret = ESP_ERR_TIMEOUT;
        goto download_emoji_cleanup;
    }

    download_result_t *results = summary->results;
    for (int t = 0; t < url_count; t++)
    {
        download_task_arg_t *task_arg = &task_args[t];
        results[t].success = task_arg->err == ESP_OK;
        results[t].error_code = task_arg->err;
    }

    int64_t total_end_time = esp_timer_get_time();
    int64_t total_time_us = total_end_time - total_start_time;
    double total_time_s = total_time_us / 1000000.0;
    double download_speed = total_data_size / total_time_s;

    ESP_LOGI(TAG, "emoji download stats, total download size: %" PRId64 " bytes", total_data_size);
    total_data_size = 0;
    ESP_LOGI(TAG, "emoji download stats, total download time: %.2f seconds", total_time_s);
    ESP_LOGI(TAG, "emoji download stats, overall download speed: %.2f bytes/second", download_speed);

    summary->total_time_us = total_time_us;
    summary->download_speed = download_speed;

download_emoji_cleanup:
    bool overall_succ = true;
    for (int i = 0; i < url_count; i++)
    {
        download_task_arg_t *task_arg = &task_args[i];
        if (task_arg->stack) free(task_arg->stack);
        if (task_arg->tcb) free(task_arg->tcb);
        if (task_arg->err != ESP_OK) overall_succ = false;
    }
    if (!overall_succ) {
        esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_EMOJI_DOWLOAD_FAILED, NULL, 0, pdMS_TO_TICKS(10000));
    }
    free(task_args);
    vEventGroupDelete(download_event_group);
    vSemaphoreDelete(download_mutex);

    return ret;
}
