// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.1
// LVGL version: 8.3.6
// Project name: watcher_example

#ifndef _WATCHER_EXAMPLE_UI_H
#define _WATCHER_EXAMPLE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

#include "ui_helpers.h"
#include "ui_events.h"

// SCREEN: ui_Page_example
void ui_Page_example_screen_init(void);
extern lv_obj_t * ui_Page_example;
void ui_event_Button1(lv_event_t * e);
extern lv_obj_t * ui_Button1;
extern lv_obj_t * ui_Label1;
void ui_event_Button2(lv_event_t * e);
extern lv_obj_t * ui_Button2;
extern lv_obj_t * ui_Label2;
void ui_event_Button3(lv_event_t * e);
extern lv_obj_t * ui_Button3;
extern lv_obj_t * ui_Label3;
extern lv_obj_t * ui____initial_actions0;


LV_IMG_DECLARE(ui_img_page_main_png);    // assets/page_main.png






void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif