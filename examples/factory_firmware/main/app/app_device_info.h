#ifndef APP_DEVICEINFO_H
#define APP_DEVICEINFO_H
#include "stdint.h"
// Your code goes here


uint8_t * get_sn(int caller);
void app_device_info_init();
char* get_software_version(int caller);
char* get_himax_software_version(int caller); 
uint8_t *get_brightness(int caller);
uint8_t *set_brightness(int caller, int value);
#endif // APP_DEVICEINFO_H