#include <time.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "sensecap-watcher.h"
#include "app_rgb.h"
#include "event_loops.h"
#include "data_defs.h"

#define RGB_TAG     "RGB_TAG"
#define MAX_CALLERS 6
#define STACK_SIZE  10

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t max_brightness_led;
    uint8_t min_brightness_led;
    int step;
    int delay_time;
    int type;
} rgb_status;

static rgb_status rgb_status_instance;
static StackType_t *app_rgb_stack = NULL;
static StaticTask_t app_rgb_stack_buffer;

typedef struct
{
    int caller;
    rgb_service_t service;
    rgb_status status;
} caller_context_t;

static caller_context_t caller_contexts[STACK_SIZE];
static int stack_top = -1; // Empty stack
SemaphoreHandle_t rgb_semaphore;
SemaphoreHandle_t __rgb_semaphore;

static esp_timer_handle_t rgb_timer_handle;
static uint8_t flag = 0;

void __blink(double interval, bool start);
void __flare();
void __set_breath_color(rgb_status *status);

/**
 * @brief Push a caller context onto the stack
 *
 * This function adds a caller context to the stack, which includes the caller ID,
 * the RGB service requested, and the current RGB status.
 *
 * @param caller The ID of the caller
 * @param service The RGB service requested
 * @param status The current RGB status
 */
void push_caller_context(int caller, rgb_service_t service, rgb_status status)
{
    if (stack_top < STACK_SIZE - 1)
    {
        stack_top++;
        caller_contexts[stack_top].caller = caller;
        caller_contexts[stack_top].service = service;
        caller_contexts[stack_top].status = status;
        ESP_LOGI(RGB_TAG, "Pushed caller %d with service %d to stack at position %d", caller, service, stack_top);
    }
    else
    {
        ESP_LOGE(RGB_TAG, "Stack overflow! Unable to push caller %d with service %d", caller, service);
    }
}

/**
 * @brief Pop a caller context from the stack
 *
 * This function removes the top caller context from the stack and returns it.
 *
 * @return The popped caller context
 */
caller_context_t pop_caller_context()
{
    caller_context_t context = { .caller = -1 };
    if (stack_top >= 0)
    {
        context = caller_contexts[stack_top];
        stack_top--;
        ESP_LOGI(RGB_TAG, "Popped caller %d with service %d from stack at position %d", context.caller, context.service, stack_top + 1);
    }
    else
    {
        ESP_LOGE(RGB_TAG, "Stack underflow! Unable to pop context");
    }
    return context;
}

/**
 * @brief Peek the top caller context on the stack
 *
 * This function returns the top caller context from the stack without removing it.
 *
 * @return The top caller context
 */
caller_context_t peek_caller_context()
{
    caller_context_t context = { .caller = -1 };
    if (stack_top >= 0)
    {
        context = caller_contexts[stack_top];
        ESP_LOGI(RGB_TAG, "Peeked caller %d with service %d from stack at position %d", context.caller, context.service, stack_top);
    }
    else
    {
        ESP_LOGI(RGB_TAG, "Stack is empty! Unable to peek context");
    }
    return context;
}

/**
 * @brief Select and set the RGB service
 *
 * This function sets the RGB status based on the service requested by the caller.
 *
 * @param caller The ID of the caller
 * @param service The RGB service requested
 */
void set_rgb_status(int r, int g, int b, int type, int step, int delay_time)
{
    // Take the semaphore to ensure thread safety
    xSemaphoreTake(__rgb_semaphore, portMAX_DELAY);
    rgb_status_instance.r = r;
    rgb_status_instance.g = g;
    rgb_status_instance.b = b;
    rgb_status_instance.type = type;
    rgb_status_instance.step = step;
    rgb_status_instance.delay_time = delay_time;
    rgb_status_instance.max_brightness_led = 255;
    rgb_status_instance.min_brightness_led = 0;
    // Release the semaphore after updating the RGB status
    xSemaphoreGive(__rgb_semaphore);
}

void __select_service_set_rgb(int caller, int service)
{
    static int __rgb_switch = 0;

    if (caller == UI_CALLER && service == on)
    {
        __rgb_switch = 1;
    }
    else if (caller == UI_CALLER && service == off)
    {
        __rgb_switch = 0;
    }

    ESP_LOGI(RGB_TAG, "Caller_inside: %d, Service_inside: %d", caller, service);

    if (__rgb_switch == 1)
    {
        switch (service)
        {
            case breath_red:
                set_rgb_status(255, 0, 0, 1, 1, 5);
                break;
            case breath_green:
                set_rgb_status(0, 255, 0, 1, 1, 5);
                break;
            case breath_blue:
                set_rgb_status(0, 0, 255, 1, 1, 5);
                break;
            case breath_white:
                set_rgb_status(255, 255, 255, 1, 1, 5);
                break;
            case glint_red:
                set_rgb_status(255, 0, 0, 2, 10, 50);
                break;
            case glint_green:
                set_rgb_status(0, 255, 0, 2, 10, 50);
                break;
            case glint_blue:
                set_rgb_status(0, 0, 255, 2, 10, 50);
                break;
            case glint_white:
                set_rgb_status(255, 255, 255, 2, 10, 50);
                break;
            case flare_red:
                set_rgb_status(255, 0, 0, 3, 5, 25);
                break;
            case flare_green:
                set_rgb_status(0, 255, 0, 3, 5, 25);
                break;
            case flare_blue:
                set_rgb_status(0, 0, 255, 3, 5, 25);
                break;
            case flare_white:
                set_rgb_status(255, 255, 255, 3, 5, 25);
                break;
            case off:
                set_rgb_status(0, 0, 0, 4, 0, 0);
                break;
            default:
                ESP_LOGW(RGB_TAG, "Unknown service: %d", service);
                set_rgb_status(0, 0, 0, 4, 0, 0);
                break;
        }
    }
    else
    {
        set_rgb_status(0, 0, 0, 4, 0, 0);
    }

    // Log the current RGB status
    ESP_LOGI(RGB_TAG, "RGB Status - R: %d, G: %d, B: %d", rgb_status_instance.r, rgb_status_instance.g, rgb_status_instance.b);
}

/**
 * @brief Set RGB status with priority
 *
 * This function sets the RGB status for a caller with priority, ensuring thread safety with a semaphore.
 *
 * @param caller The ID of the caller
 * @param service The RGB service requested
 */
void set_rgb_with_priority(int caller, int service)
{
    if (caller < 0 || caller >= MAX_CALLERS)
    {
        ESP_LOGE(RGB_TAG, "Invalid caller: %d", caller);
        return;
    }

    ESP_LOGI(RGB_TAG, "Caller: %d, Service: %d", caller, service);

    xSemaphoreTake(rgb_semaphore, portMAX_DELAY);

    // Save current status before changing
    // push_caller_context(caller, service, rgb_status_instance);

    // Set new status
    __select_service_set_rgb(caller, service);

    xSemaphoreGive(rgb_semaphore);
}

/**
 * @brief Set breath color effect
 *
 * This function sets the RGB light to perform a breathing effect with the specified color.
 *
 * @param status The current RGB status
 */
void __set_breath_color(rgb_status *status)
{
    static uint8_t brightness_led = 0;
    static bool increasing = true;

    if (increasing)
    {
        brightness_led += status->step;
        if (brightness_led >= status->max_brightness_led)
        {
            brightness_led = status->max_brightness_led;
            increasing = false;
        }
    }
    else
    {
        brightness_led -= status->step;
        if (brightness_led <= status->min_brightness_led)
        {
            brightness_led = status->min_brightness_led;
            increasing = true;
        }
    }

    uint8_t current_r = (status->r * brightness_led) / 255;
    uint8_t current_g = (status->g * brightness_led) / 255;
    uint8_t current_b = (status->b * brightness_led) / 255;
    bsp_rgb_set(current_r, current_g, current_b);

    vTaskDelay(pdMS_TO_TICKS(status->delay_time));
}

/**
 * @brief Blink effect
 *
 * This function starts or stops the blink effect based on the start parameter.
 *
 * @param interval The interval for the blink effect in seconds
 * @param start True to start the effect, false to stop
 */

static void blink_timer_callback(void *arg)
{
    static bool led_on = false;
    if (led_on)
    {
        bsp_rgb_set(0, 0, 0);
    }
    else
    {
        bsp_rgb_set(rgb_status_instance.r, rgb_status_instance.g, rgb_status_instance.b);
    }
    led_on = !led_on;
}

void __blink(double interval, bool start)
{
    static bool is_blinking = false;
    static esp_timer_handle_t blink_timer_handle = NULL;
    vTaskDelay(pdMS_TO_TICKS(5));
    if (start)
    {
        if (!is_blinking)
        {
            is_blinking = true;
            esp_timer_create_args_t timer_args = { .callback = &blink_timer_callback, .arg = NULL, .name = "blink_timer" };
            esp_timer_create(&timer_args, &blink_timer_handle);
            esp_timer_start_periodic(blink_timer_handle, interval * 1000000 * 0.5);
        }
    }
    else
    {
        if (is_blinking)
        {
            is_blinking = false;
            esp_timer_stop(blink_timer_handle);
            // esp_timer_delete(blink_timer_handle);
            blink_timer_handle = NULL;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
}

/**
 * @brief Flare effect
 *
 * This function sets the RGB light to perform a flare effect.
 */
void __flare()
{
    vTaskDelay(pdMS_TO_TICKS(5));
    bsp_rgb_set(rgb_status_instance.r, rgb_status_instance.g, rgb_status_instance.b);
    vTaskDelay(pdMS_TO_TICKS(5));
}

/**
 * @brief RGB breath effect task
 *
 * This task handles the breath effect of the RGB lights.
 *
 * @param arg Task argument (not used)
 */
void breath_effect_task(void *arg)
{
    while (true)
    {
        // Take the semaphore to ensure thread safety
        xSemaphoreTake(__rgb_semaphore, portMAX_DELAY);
        switch (rgb_status_instance.type)
        {
            case 1:
                __set_breath_color(&rgb_status_instance);
                break;
            case 2:
                __blink(1, true);
                break;
            case 3:
                __flare();
                break;
            case 4:
                vTaskDelay(pdMS_TO_TICKS(10));
                bsp_rgb_set(rgb_status_instance.r, rgb_status_instance.g, rgb_status_instance.b);
                __blink(1, false);
                vTaskDelay(pdMS_TO_TICKS(5));
                break;
            default:
                vTaskDelay(pdMS_TO_TICKS(10));
                bsp_rgb_set(0, 0, 0);
                __blink(1, false);
                vTaskDelay(pdMS_TO_TICKS(5));
                break;
        }
        xSemaphoreGive(__rgb_semaphore);
        vTaskDelay(pdMS_TO_TICKS(100));
        // Release the semaphore after updating the RGB status
    }
}

/**
 * @brief Initialize the RGB application
 *
 * This function initializes the RGB application, creating necessary tasks and resources.
 *
 * @return 0 on success, -1 on failure
 */
int app_rgb_init(void)
{
    rgb_status_instance = (rgb_status) { .r = 255, .g = 255, .b = 255, .max_brightness_led = 255, .min_brightness_led = 0, .step = 50, .delay_time = 200 };

    rgb_semaphore = xSemaphoreCreateMutex();
    __rgb_semaphore = xSemaphoreCreateMutex();
    if (rgb_semaphore == NULL)
    {
        ESP_LOGE(RGB_TAG, "Failed to create semaphore");
        return -1;
    }

    app_rgb_stack = (StackType_t *)heap_caps_malloc(4096 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
    if (app_rgb_stack == NULL)
    {
        ESP_LOGE(RGB_TAG, "Failed to allocate memory for task stack");
        return -1;
    }
    TaskHandle_t task_handle = xTaskCreateStatic(&breath_effect_task, "app_rgb_task", 4096, &rgb_status_instance, 3, app_rgb_stack, &app_rgb_stack_buffer);
    if (task_handle == NULL)
    {
        ESP_LOGE(RGB_TAG, "Failed to create task");
        return -1;
    }

    return 0;
}
