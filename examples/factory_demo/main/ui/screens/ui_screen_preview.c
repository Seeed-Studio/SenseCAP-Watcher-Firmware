// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.0
// LVGL version: 8.3.6
// Project name: indoor_combo

#include "../ui.h"

void ui_screen_preview_screen_init(void)
{
ui_screen_preview = lv_obj_create(NULL);
lv_obj_clear_flag( ui_screen_preview, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

lv_obj_add_event_cb(ui_screen_preview, ui_event_screen_preview, LV_EVENT_ALL, NULL);

}