// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: SenseCAP-Watcher

#include "../ui.h"

void ui_Page_Guideavatar_screen_init(void)
{
    ui_Page_Guideavatar = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Page_Guideavatar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Page_Guideavatar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Page_Guideavatar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1p1 = lv_obj_create(ui_Page_Guideavatar);
    lv_obj_set_width(ui_guide1p1, 412);
    lv_obj_set_height(ui_guide1p1, 412);
    lv_obj_set_align(ui_guide1p1, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_guide1p1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_guide1p1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_guide1p1, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_guide1p1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_guide1p1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1img1 = lv_img_create(ui_guide1p1);
    lv_img_set_src(ui_guide1img1, &ui_img_guideimg_png);
    lv_obj_set_width(ui_guide1img1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_guide1img1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_guide1img1, 0);
    lv_obj_set_y(ui_guide1img1, -39);
    lv_obj_set_align(ui_guide1img1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_guide1img1, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_guide1img1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_guide1t1 = lv_label_create(ui_guide1p1);
    lv_obj_set_width(ui_guide1t1, 283);
    lv_obj_set_height(ui_guide1t1, 96);
    lv_obj_set_x(ui_guide1t1, 0);
    lv_obj_set_y(ui_guide1t1, 106);
    lv_obj_set_align(ui_guide1t1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_guide1t1, "Display your own avatar when watching the space");
    lv_obj_set_style_text_color(ui_guide1t1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_guide1t1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_guide1t1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_guide1t1, &ui_font_fbold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1p2 = lv_obj_create(ui_Page_Guideavatar);
    lv_obj_set_width(ui_guide1p2, 412);
    lv_obj_set_height(ui_guide1p2, 412);
    lv_obj_set_align(ui_guide1p2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_guide1p2, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_guide1p2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_guide1p2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_guide1p2, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_guide1p2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_guide1p2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1img2 = lv_img_create(ui_guide1p2);
    lv_img_set_src(ui_guide1img2, &ui_img_scroll_down_png);
    lv_obj_set_width(ui_guide1img2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_guide1img2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_guide1img2, 43);
    lv_obj_set_y(ui_guide1img2, -66);
    lv_obj_set_align(ui_guide1img2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_guide1img2, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_guide1img2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_guide1t2 = lv_label_create(ui_guide1p2);
    lv_obj_set_width(ui_guide1t2, 270);
    lv_obj_set_height(ui_guide1t2, 81);
    lv_obj_set_x(ui_guide1t2, 0);
    lv_obj_set_y(ui_guide1t2, 120);
    lv_obj_set_align(ui_guide1t2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_guide1t2, "Scroll down to display live cam");
    lv_obj_set_style_text_color(ui_guide1t2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_guide1t2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_guide1t2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_guide1t2, &ui_font_fbold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1p3 = lv_obj_create(ui_Page_Guideavatar);
    lv_obj_set_width(ui_guide1p3, 412);
    lv_obj_set_height(ui_guide1p3, 412);
    lv_obj_set_align(ui_guide1p3, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_guide1p3, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_guide1p3, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_guide1p3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_guide1p3, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_guide1p3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_guide1p3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1btn1 = lv_btn_create(ui_guide1p3);
    lv_obj_set_width(ui_guide1btn1, 270);
    lv_obj_set_height(ui_guide1btn1, 75);
    lv_obj_set_x(ui_guide1btn1, 0);
    lv_obj_set_y(ui_guide1btn1, -87);
    lv_obj_set_align(ui_guide1btn1, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_guide1btn1, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_guide1btn1, lv_color_hex(0x8FC31F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_guide1btn1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_guide1btn1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_guide1btn1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1btn1t = lv_label_create(ui_guide1btn1);
    lv_obj_set_width(ui_guide1btn1t, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_guide1btn1t, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_guide1btn1t, LV_ALIGN_CENTER);
    lv_label_set_text(ui_guide1btn1t, "Main Menu");
    lv_obj_set_style_text_font(ui_guide1btn1t, &ui_font_font_bold, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1btn2 = lv_btn_create(ui_guide1p3);
    lv_obj_set_width(ui_guide1btn2, 270);
    lv_obj_set_height(ui_guide1btn2, 75);
    lv_obj_set_x(ui_guide1btn2, 0);
    lv_obj_set_y(ui_guide1btn2, 19);
    lv_obj_set_align(ui_guide1btn2, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_guide1btn2, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_guide1btn2, lv_color_hex(0xD54941), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_guide1btn2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_guide1btn2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_guide1btn2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1btn2t = lv_label_create(ui_guide1btn2);
    lv_obj_set_width(ui_guide1btn2t, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_guide1btn2t, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_guide1btn2t, LV_ALIGN_CENTER);
    lv_label_set_text(ui_guide1btn2t, "End Task");
    lv_obj_set_style_text_font(ui_guide1btn2t, &ui_font_font_bold, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_guide1btn3 = lv_btn_create(ui_guide1p3);
    lv_obj_set_width(ui_guide1btn3, 90);
    lv_obj_set_height(ui_guide1btn3, 90);
    lv_obj_set_x(ui_guide1btn3, 0);
    lv_obj_set_y(ui_guide1btn3, 135);
    lv_obj_set_align(ui_guide1btn3, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_guide1btn3, 59, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_guide1btn3, lv_color_hex(0x202124), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_guide1btn3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_guide1btn3, &ui_img_setback_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_guide1btn3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_guide1btn3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_guide1p1, ui_event_guide1p1, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_guide1p2, ui_event_guide1p2, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_guide1btn1, ui_event_guide1btn1, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_guide1btn2, ui_event_guide1btn2, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_guide1btn3, ui_event_guide1btn3, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_guide1p3, ui_event_guide1p3, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Page_Guideavatar, ui_event_Page_Guideavatar, LV_EVENT_ALL, NULL);

}
