// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ai

#include "../ui.h"

void ui_ltask_view_screen_init(void)
{
    ui_ltask_view = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ltask_view, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_add_event_cb(ui_ltask_view, ui_event_ltask_view, LV_EVENT_ALL, NULL);

}