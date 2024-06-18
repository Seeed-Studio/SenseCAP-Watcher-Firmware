#ifndef EVENT_H
#define EVENT_H

#include "ui/ui.h"

extern lv_obj_t *ui_Left1;
extern lv_obj_t *ui_Left2;
extern lv_obj_t *ui_Left3;
extern lv_obj_t *ui_Left4;
extern lv_obj_t *ui_Left5;
extern lv_obj_t *ui_Left6;
extern lv_obj_t *ui_Left7;
extern lv_obj_t *ui_Left8;
extern lv_obj_t *ui_Right1;
extern lv_obj_t *ui_Right2;
extern lv_obj_t *ui_Right3;
extern lv_obj_t *ui_Right4;
extern lv_obj_t *ui_Right5;
extern lv_obj_t *ui_Right6;
extern lv_obj_t *ui_Right7;
extern lv_obj_t *ui_Right8;

// taskflow error msg event
void taskerrc_cb(lv_event_t *e);

// Wi-Fi config status
void waitForWifi();
void waitForBinding();
void waitForAddDev();
void bindFinish();
void wifiConnectFailed();

// view alarm panel event
void ui_event_alarm_panel(lv_event_t * e);

void viewInfoInit();
void view_info_obtain();

#endif