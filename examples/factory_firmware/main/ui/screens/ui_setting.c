// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ai

#include "../ui.h"

void ui_setting_screen_init(void)
{
    ui_setting = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_setting, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_setting, lv_color_hex(0x080808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_setting, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_setting, &ui_img_setting__png, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_setting, ui_event_setting, LV_EVENT_ALL, NULL);

}