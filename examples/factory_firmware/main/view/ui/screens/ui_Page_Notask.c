// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.1
// LVGL version: 8.3.6
// Project name: SenseCAP-Watcher

#include "../ui.h"

void ui_Page_Notask_screen_init(void)
{
    ui_Page_Notask = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Page_Notask, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Page_Notask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Page_Notask, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_notasktext = lv_label_create(ui_Page_Notask);
    lv_obj_set_width(ui_notasktext, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_notasktext, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_notasktext, 0);
    lv_obj_set_y(ui_notasktext, -80);
    lv_obj_set_align(ui_notasktext, LV_ALIGN_CENTER);
    lv_label_set_text(ui_notasktext, "No current task");
    lv_obj_set_style_text_color(ui_notasktext, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_notasktext, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_notasktext, &ui_font_font_bold, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_curtask1b = lv_btn_create(ui_Page_Notask);
    lv_obj_set_width(ui_curtask1b, 60);
    lv_obj_set_height(ui_curtask1b, 60);
    lv_obj_set_x(ui_curtask1b, 0);
    lv_obj_set_y(ui_curtask1b, 120);
    lv_obj_set_align(ui_curtask1b, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_curtask1b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_curtask1b, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_curtask1b, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_curtask1b, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_curtask1b, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_curtask1b, &ui_img_button_cancel_png, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label3 = lv_label_create(ui_Page_Notask);
    lv_obj_set_width(ui_Label3, 295);
    lv_obj_set_height(ui_Label3, 84);
    lv_obj_set_x(ui_Label3, 0);
    lv_obj_set_y(ui_Label3, 43);
    lv_obj_set_align(ui_Label3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label3, "You can long press the wheel or use App to send new task");
    lv_obj_set_style_text_color(ui_Label3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Label3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label3, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_curtask1b, ui_event_curtask1b, LV_EVENT_ALL, NULL);

}