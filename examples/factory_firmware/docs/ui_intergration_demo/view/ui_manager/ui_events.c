// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ai
#include "esp_log.h"
#include "data_defs.h"
#include "event_loops.h"
#include "esp_wifi.h"
#include <dirent.h>
#include "esp_timer.h"

#include "ui/ui.h"
#include "view.h"
#include "pm.h"
#include "animation.h"
#include "event.h"

#include "app_device_info.h"
#include "app_ble.h"
#include "app_wifi.h"
#include "storage.h"
#include "app_png.h"
#include "sensecap-watcher.h"

static const char *TAG = "ui_event";
static const char *CLICK_TAG = "Click_event";

extern GroupInfo group_page_example;

void btn1click_cb(lv_event_t * e)
{
    ESP_LOGI("ui_example", "btn1click_cb");
}
void btn2click_cb(lv_event_t * e)
{
    ESP_LOGI("ui_example", "btn2click_cb");
}
void btn3click_cb(lv_event_t * e)
{
    ESP_LOGI("ui_example", "btn3click_cb");
}