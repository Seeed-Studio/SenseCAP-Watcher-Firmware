#include <time.h>

#include "esp_timer.h"

#include "indoor_ai_camera.h"

#include "app_rgb.h"
#include "event_loops.h"
#include "data_defs.h"

static const char *TAG = "rgb";

static esp_timer_handle_t     rgb_timer_handle;

static uint8_t flag = 0;
static void __timer_callback(void* arg)
{
    
    if( flag ) {
        bsp_rgb_set(0, 0, 0);
    } else {
        bsp_rgb_set(255, 0, 0);
    }
    flag = !flag;
}

static void __view_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    switch (id)
    {
        case VIEW_EVENT_ALARM_ON:
        {
            ESP_LOGI(TAG, "event: VIEW_EVENT_ALARM_ON");
            esp_timer_stop(rgb_timer_handle);
            esp_timer_start_periodic(rgb_timer_handle, 1000000 * 0.5);

            break;
        }
        case VIEW_EVENT_ALARM_OFF:
        {
            ESP_LOGI(TAG, "event: VIEW_EVENT_ALARM_OFF");
            esp_timer_stop(rgb_timer_handle);
            bsp_rgb_set(0, 0, 0);
            break;
        }
        case VIEW_EVENT_IMAGE_640_480_SEND:
        {
            ESP_LOGI(TAG, "event: VIEW_EVENT_IMAGE_640_480_SEND");
            esp_timer_stop(rgb_timer_handle);
            flag=1;
            bsp_rgb_set(255, 0, 0);
            esp_timer_start_once(rgb_timer_handle, (uint64_t) 0.5 * 1000000 );
            break;
        }
    default:
        break;
    }
}

int app_rgb_init(void)
{

    const esp_timer_create_args_t timer_args = {
            .callback = &__timer_callback,
            /* argument specified here will be passed to timer callback function */
            .arg = (void*) rgb_timer_handle,
            .name = "rgb timer"
    };
    ESP_ERROR_CHECK( esp_timer_create(&timer_args, &rgb_timer_handle));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_ALARM_ON, 
                                                            __view_event_handler, NULL, NULL));   

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_ALARM_OFF, 
                                                            __view_event_handler, NULL, NULL));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_IMAGE_640_480_SEND, 
                                                            __view_event_handler, NULL, NULL));
    return 0;
}