// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ai

#include "../ui.h"

void ui_waitingtask_screen_init(void)
{
    ui_waitingtask = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_waitingtask, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_waittext = lv_label_create(ui_waitingtask);
    lv_obj_set_width(ui_waittext, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_waittext, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_waittext, 0);
    lv_obj_set_y(ui_waittext, 30);
    lv_obj_set_align(ui_waittext, LV_ALIGN_CENTER);
    lv_label_set_text(ui_waittext, "waiting for task\n      from app");
    lv_obj_set_style_text_font(ui_waittext, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_waitspinner = lv_spinner_create(ui_waitingtask, 1000, 90);
    lv_obj_set_width(ui_waitspinner, 20);
    lv_obj_set_height(ui_waitspinner, 20);
    lv_obj_set_x(ui_waitspinner, 0);
    lv_obj_set_y(ui_waitspinner, -30);
    lv_obj_set_align(ui_waitspinner, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_waitspinner, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_arc_width(ui_waitspinner, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_width(ui_waitspinner, 5, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_waitingtask, ui_event_waitingtask, LV_EVENT_ALL, NULL);

}