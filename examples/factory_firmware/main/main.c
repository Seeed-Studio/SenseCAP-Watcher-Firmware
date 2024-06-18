#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_app_desc.h"
#include "cJSON.h"
#include "esp_heap_task_info.h"
#include "factory_info.h"

#include "sensecap-watcher.h"

#include "event_loops.h"
#include "data_defs.h"
#include "storage.h"
#include "audio_player.h"
#include "app_sr.h"
#include "app_audio.h"
#include "app_wifi.h"
#include "app_ble.h"
#include "app_time.h"
#include "app_cmd.h"
#include "at_cmd.h"
#include "app_sensecraft.h"
#include "app_rgb.h"
#include "app_device_info.h"
#include "util.h"
#include "app_ota.h"
#include "app_taskflow.h"
#include "view.h"


#ifdef CONFIG_INTR_TRACKING
#include "esp_intr_types.h"
#endif

static const char *TAG = "app_main";

#define SENSECAP                                                                                                                                                                                       \
    "\n\
   _____                      _________    ____         \n\
  / ___/___  ____  ________  / ____/   |  / __ \\       \n\
  \\__ \\/ _ \\/ __ \\/ ___/ _ \\/ /   / /| | / /_/ /   \n\
 ___/ /  __/ / / (__  )  __/ /___/ ___ |/ ____/         \n\
/____/\\___/_/ /_/____/\\___/\\____/_/  |_/_/     WATCHER\n\
--------------------------------------------------------\n\
 Version: %s %s %s\n\
--------------------------------------------------------\n\
"

ESP_EVENT_DEFINE_BASE(VIEW_EVENT_BASE);
ESP_EVENT_DEFINE_BASE(CTRL_EVENT_BASE);
esp_event_loop_handle_t app_event_loop_handle;

#ifdef CONFIG_HEAP_TASK_TRACKING
#define MAX_TASK_NUM  30  // Max number of per tasks info that it can store
#define MAX_BLOCK_NUM 100 // Max number of per block info that it can store

static size_t s_prepopulated_num = 0;
static heap_task_totals_t s_totals_arr[MAX_TASK_NUM];
static heap_task_block_t s_block_arr[MAX_BLOCK_NUM];
#endif

static void *__cJSON_malloc(size_t sz)
{
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
}

static void __cJSON_free(void *ptr)
{
    free(ptr);
}

cJSON_Hooks cJSONHooks = { .malloc_fn = __cJSON_malloc, .free_fn = __cJSON_free };

static void __app_event_loop_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch (id)
    {
        case VIEW_EVENT_SHUTDOWN: {
            ESP_LOGI(TAG, "event: VIEW_EVENT_SHUTDOWN");
            app_sensecraft_disconnect();
            bsp_lcd_brightness_set(0);
            for (int i = 0; i < 10; i++)
            {
                vTaskDelay(pdMS_TO_TICKS(200));
                if (!app_sensecraft_is_connected())
                    break;
            }
            fflush(stdout);
            if (get_sdcard_total_size(MAX_CALLER) > 0)
            {
                bsp_sdcard_deinit_default();
            }
            if (get_spiffs_total_size(MAX_CALLER) > 0)
            {
                esp_vfs_spiffs_unregister("storage");
            }
            bsp_system_shutdown();
            break;
        }
        case VIEW_EVENT_REBOOT: {
            ESP_LOGI(TAG, "event: VIEW_EVENT_REBOOT");
            app_sensecraft_disconnect();
            bsp_lcd_brightness_set(0);
            fflush(stdout);
            if (get_sdcard_total_size(MAX_CALLER) > 0)
            {
                bsp_sdcard_deinit_default();
            }
            if (get_spiffs_total_size(MAX_CALLER) > 0)
            {
                esp_vfs_spiffs_unregister("storage");
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            esp_restart();
            break;
        }
        default:
            break;
    }
}

void board_init(void)
{
    sscma_client_handle_t sscma_client;

    // nvs key-value storage
    storage_init();
    factory_info_init();
    bsp_spiffs_init(DRV_BASE_PATH_FLASH, 100);
    bsp_io_expander_init();
    if (bsp_sdcard_is_inserted())
    {
        bsp_sdcard_init_default();
    }
    lv_disp_t *lvgl_disp = bsp_lvgl_init();
    assert(lvgl_disp != NULL);
    bsp_rgb_init();
    bsp_codec_init();

    sscma_client = bsp_sscma_client_init();
    if (sscma_client)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(sscma_client_init(sscma_client));
    }

    bsp_rtc_init();

    bsp_codec_volume_set(100, NULL);
    // audio_play_task("/spiffs/echo_en_wake.wav");

    app_device_info_init_early();
}


void app_init(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    audio_player_init();
    app_rgb_init();
    app_device_info_init();
    app_sensecraft_init();
    app_ota_init();
    app_taskflow_init();
    app_wifi_init();
    app_time_init();
    app_at_cmd_init();
    app_ble_init();
    app_cmd_init();
    // app_sr_start(false);
}

void task_app_init(void *p_arg)
{
    board_init();
    view_init();

    app_init();

    esp_event_handler_register_with(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_SHUTDOWN, __app_event_loop_handler, NULL);
    esp_event_handler_register_with(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_REBOOT, __app_event_loop_handler, NULL);

    vTaskDelete(NULL);
}

#ifdef CONFIG_HEAP_TASK_TRACKING
/**
 * IDF v5.2.1 has issue on this, should apply https://github.com/espressif/esp-idf/commit/26160a217e3a2953fd0b2eabbde1075e3bb46941
 * manually, we don't wanna upgrade IDF version for this only issue though.
 */
static void esp_dump_per_task_heap_info(void)
{
    heap_task_info_params_t heap_info = { 0 };
    heap_info.caps[0] = MALLOC_CAP_INTERNAL; // Gets heap with CAP_INTERNAL capabilities
    heap_info.mask[0] = MALLOC_CAP_INTERNAL;
    heap_info.caps[1] = MALLOC_CAP_SPIRAM; // Gets heap info with CAP_SPIRAM capabilities
    heap_info.mask[1] = MALLOC_CAP_SPIRAM;
    heap_info.tasks = NULL; // Passing NULL captures heap info for all tasks
    heap_info.num_tasks = 0;
    heap_info.totals = s_totals_arr; // Gets task wise allocation details
    heap_info.num_totals = &s_prepopulated_num;
    heap_info.max_totals = MAX_TASK_NUM;  // Maximum length of "s_totals_arr"
    heap_info.blocks = s_block_arr;       // Gets block wise allocation details. For each block, gets owner task, address and size
    heap_info.max_blocks = MAX_BLOCK_NUM; // Maximum length of "s_block_arr"

    heap_caps_get_per_task_info(&heap_info);

    for (int i = 0; i < *heap_info.num_totals; i++)
    {
        printf("Task: %s -> CAP_INTERNAL: %d CAP_SPIRAM: %d\n", heap_info.totals[i].task ? pcTaskGetName(heap_info.totals[i].task) : "Pre-Scheduler allocs", heap_info.totals[i].size[0],
            heap_info.totals[i].size[1]);
    }

    printf("\n\n");
}
#endif

void app_main(void)
{
#if CONFIG_ENABLE_FACTORY_FW_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("MEM", ESP_LOG_DEBUG);
#endif

    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI("", SENSECAP, app_desc->version, __DATE__, __TIME__);

    cJSON_InitHooks(&cJSONHooks);

    esp_event_loop_args_t app_event_loop_args = { .queue_size = 64,
        .task_name = "app_eventloop",
        .task_priority = 15, // uxTaskPriorityGet(NULL),
        .task_stack_size = 1024 * 4,
        .task_core_id = 0 };
    ESP_ERROR_CHECK(esp_event_loop_create(&app_event_loop_args, &app_event_loop_handle));

    // app modules init
    xTaskCreatePinnedToCore(task_app_init, "task_app_init", 4096, NULL, 5, NULL, 1);

    static char buffer[512];
    while (1)
    {
        sprintf(buffer,
            "    Biggest /  Minimum /     Free /    Total\n"
            "\t  DRAM : [%8d / %8d / %8d / %8d]\n"
            "\t  PSRAM: [%8d / %8d / %8d / %8d]\n"
            "\t  DMA  : [%8d / %8d / %8d / %8d]",
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_total_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_total_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_DMA), heap_caps_get_minimum_free_size(MALLOC_CAP_DMA),
            heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_total_size(MALLOC_CAP_DMA));

        ESP_LOGI("MEM", "%s\n", buffer);

#ifdef CONFIG_INTR_TRACKING
        esp_intr_dump(stdout);
#endif

        /**
         * requires configuration:
         * Component config -> Heap memory debugging -> Heap corruption detection (Light Impact)
         * Component config -> Heap memory debugging -> Enable heap task tracking (Yes)
         */
#ifdef CONFIG_HEAP_TASK_TRACKING
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_dump_per_task_heap_info();
#endif

        /**
         * requires configuration:
         * Component config -> FreeRTOS -> Kernel -> configUSE_TRACE_FACILITY (Yes)
         *                                           configUSE_STATS_FORMATTING_FUNCTIONS (Yes)
         */
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
        vTaskDelay(pdMS_TO_TICKS(1));
        vTaskList(buffer);
        ESP_LOGI("task stack", "\nTask Name       Status  Prio    HWM     Task#\n%s\n", buffer);
#endif

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}