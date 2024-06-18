#pragma once
#include <stdint.h>
#include <esp_err.h>
#include "data_defs.h"


#define DEVICEINFO_STORAGE  "deviceinfo"

enum { BLE_CONNECTED, BLE_DISCONNECTED, STATUS_WAITTING };
enum {BLE_SWITCH_OFF,BLE_SWITCH_ON,BLE_SWITCH_DANGLING};
uint8_t *get_sn(int caller);
uint8_t *get_eui();
uint8_t *get_qrcode_content();
uint8_t *get_bt_mac();
uint8_t *get_wifi_mac();
char *get_software_version(int caller);
char *get_himax_software_version(int caller);

int get_brightness(int caller);
esp_err_t set_brightness(int caller, int value);
int get_rgb_switch(int caller);
esp_err_t set_rgb_switch(int caller, int value);
int get_sound(int caller);
esp_err_t set_sound(int caller, int value);
int get_cloud_service_switch(int caller);
esp_err_t set_cloud_service_switch(int caller, int value);
int get_usage_guide(int caller);
esp_err_t set_usage_guide(int caller, int value);
esp_err_t set_reset_factory();

/**
 * will be deprecated!
*/
int get_time_automatic(int caller);
esp_err_t set_time_automatic(int caller, int value);

/**
 * all the following size unit is KiB.
*/
uint16_t get_spiffs_total_size(int caller);
uint16_t get_spiffs_free_size(int caller);
/**
 * all the following size unit is MiB.
*/
uint16_t get_sdcard_total_size(int caller);
uint16_t get_sdcard_free_size(int caller);

void app_device_info_init_early();
void app_device_info_init();

