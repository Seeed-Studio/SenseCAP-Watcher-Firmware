// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: SenseCAP-Watcher

#include "../ui.h"

void ui_Page_ViewAva_screen_init(void)
{
    ui_Page_ViewAva = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Page_ViewAva, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Page_ViewAva, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Page_ViewAva, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_Page_ViewAva, ui_event_Page_ViewAva, LV_EVENT_ALL, NULL);

}
