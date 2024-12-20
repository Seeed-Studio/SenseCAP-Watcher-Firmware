// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: SenseCAP-Watcher

#include "../ui.h"

void ui_Page_Revtask_screen_init(void)
{
    ui_Page_Revtask = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Page_Revtask, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Page_Revtask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Page_Revtask, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_revtext = lv_label_create(ui_Page_Revtask);
    lv_obj_set_width(ui_revtext, 360);
    lv_obj_set_height(ui_revtext, 100);
    lv_obj_set_x(ui_revtext, 0);
    lv_obj_set_y(ui_revtext, 18);
    lv_obj_set_align(ui_revtext, LV_ALIGN_CENTER);
    lv_label_set_text(ui_revtext, "Receiving \nTask...");
    lv_obj_set_style_text_color(ui_revtext, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_revtext, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_revtext, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_revtext, &ui_font_font_bold, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Spinner3 = lv_spinner_create(ui_Page_Revtask, 1000, 90);
    lv_obj_set_width(ui_Spinner3, 50);
    lv_obj_set_height(ui_Spinner3, 50);
    lv_obj_set_x(ui_Spinner3, 0);
    lv_obj_set_y(ui_Spinner3, -100);
    lv_obj_set_align(ui_Spinner3, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Spinner3, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_arc_width(ui_Spinner3, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(ui_Spinner3, lv_color_hex(0xA1D42A), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_Spinner3, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_Spinner3, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

}
