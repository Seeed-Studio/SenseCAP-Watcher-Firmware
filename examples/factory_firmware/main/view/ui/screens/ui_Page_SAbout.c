// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ai

#include "../ui.h"

void ui_Page_SAbout_screen_init(void)
{
    ui_Page_SAbout = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Page_SAbout, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Page_SAbout, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Page_SAbout, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_AboutP = lv_obj_create(ui_Page_SAbout);
    lv_obj_set_width(ui_AboutP, 348);
    lv_obj_set_height(ui_AboutP, 356);
    lv_obj_set_x(ui_AboutP, 0);
    lv_obj_set_y(ui_AboutP, 24);
    lv_obj_set_align(ui_AboutP, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_AboutP, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_AboutP, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_AboutP, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_SCROLL_ONE);     /// Flags
    lv_obj_set_scroll_dir(ui_AboutP, LV_DIR_VER);
    lv_obj_set_style_radius(ui_AboutP, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_AboutP, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AboutP, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_AboutP, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_AboutP, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_AboutP, lv_color_hex(0xFFFFFF), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AboutP, 255, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_AboutP, 5, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_AboutP, 20, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_AboutP, 60, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_AboutP, 100, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    ui_aboutdn = lv_obj_create(ui_AboutP);
    lv_obj_set_width(ui_aboutdn, 300);
    lv_obj_set_height(ui_aboutdn, 80);
    lv_obj_set_align(ui_aboutdn, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_aboutdn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_aboutdn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_aboutdn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_aboutdn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_aboutdn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_aboutdn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_aboutdn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_aboutdn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_dnt1 = lv_label_create(ui_aboutdn);
    lv_obj_set_width(ui_dnt1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_dnt1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_dnt1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_dnt1, "Device Name :");
    lv_obj_set_style_text_color(ui_dnt1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_dnt1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_dnt1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_dnt2 = lv_label_create(ui_aboutdn);
    lv_obj_set_width(ui_dnt2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_dnt2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_dnt2, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_dnt2, "Sensecap Watcher");
    lv_obj_add_flag(ui_dnt2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_color(ui_dnt2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_dnt2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_dnt2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_aboutsv = lv_obj_create(ui_AboutP);
    lv_obj_set_width(ui_aboutsv, 300);
    lv_obj_set_height(ui_aboutsv, 158);
    lv_obj_set_align(ui_aboutsv, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_aboutsv, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_aboutsv, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_aboutsv, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_aboutsv, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_aboutsv, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_aboutsv, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_aboutsv, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_aboutsv, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_svt1 = lv_label_create(ui_aboutsv);
    lv_obj_set_width(ui_svt1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_svt1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_svt1, 1);
    lv_obj_set_y(ui_svt1, 0);
    lv_obj_set_align(ui_svt1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_svt1, "Software Version :");
    lv_obj_set_style_text_color(ui_svt1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_svt1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_svt1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_svt2 = lv_label_create(ui_aboutsv);
    lv_obj_set_width(ui_svt2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_svt2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_svt2, 1);
    lv_obj_set_y(ui_svt2, 0);
    lv_obj_set_align(ui_svt2, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_svt2, "1.0");
    lv_obj_add_flag(ui_svt2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_color(ui_svt2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_svt2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_svt2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_aboutsbb = lv_btn_create(ui_aboutsv);
    lv_obj_set_width(ui_aboutsbb, 250);
    lv_obj_set_height(ui_aboutsbb, 60);
    lv_obj_set_align(ui_aboutsbb, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_aboutsbb, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_aboutsbb, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_aboutsbb, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_aboutsbb, lv_color_hex(0x91BF25), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_aboutsbb, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_sbbtext = lv_label_create(ui_aboutsbb);
    lv_obj_set_width(ui_sbbtext, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_sbbtext, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_sbbtext, LV_ALIGN_CENTER);
    lv_label_set_text(ui_sbbtext, "Update");
    lv_obj_set_style_text_font(ui_sbbtext, &ui_font_fontbold26, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_aboutsn = lv_obj_create(ui_AboutP);
    lv_obj_set_width(ui_aboutsn, 300);
    lv_obj_set_height(ui_aboutsn, 80);
    lv_obj_set_align(ui_aboutsn, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_aboutsn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_aboutsn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_aboutsn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_aboutsn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_aboutsn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_aboutsn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_aboutsn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_aboutsn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_snt1 = lv_label_create(ui_aboutsn);
    lv_obj_set_width(ui_snt1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_snt1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_snt1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_snt1, "S/N :");
    lv_obj_set_style_text_color(ui_snt1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_snt1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_snt1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_snt2 = lv_label_create(ui_aboutsn);
    lv_obj_set_width(ui_snt2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_snt2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_snt2, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_snt2, "114992872233500085\n");
    lv_obj_add_flag(ui_snt2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_color(ui_snt2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_snt2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_snt2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_abouteui = lv_obj_create(ui_AboutP);
    lv_obj_set_width(ui_abouteui, 300);
    lv_obj_set_height(ui_abouteui, 80);
    lv_obj_set_align(ui_abouteui, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_abouteui, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_abouteui, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_abouteui, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_abouteui, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_abouteui, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_abouteui, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_abouteui, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_abouteui, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_euit1 = lv_label_create(ui_abouteui);
    lv_obj_set_width(ui_euit1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_euit1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_euit1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_euit1, "EUI :");
    lv_obj_set_style_text_color(ui_euit1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_euit1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_euit1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_euit2 = lv_label_create(ui_abouteui);
    lv_obj_set_width(ui_euit2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_euit2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_euit2, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_euit2, "114992872233500085");
    lv_obj_add_flag(ui_euit2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_color(ui_euit2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_euit2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_euit2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_aboutble = lv_obj_create(ui_AboutP);
    lv_obj_set_width(ui_aboutble, 300);
    lv_obj_set_height(ui_aboutble, 80);
    lv_obj_set_align(ui_aboutble, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_aboutble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_aboutble, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_aboutble, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_aboutble, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_aboutble, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_aboutble, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_aboutble, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_aboutble, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_blet1 = lv_label_create(ui_aboutble);
    lv_obj_set_width(ui_blet1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_blet1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_blet1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_blet1, "BT :");
    lv_obj_set_style_text_color(ui_blet1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_blet1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_blet1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_blet2 = lv_label_create(ui_aboutble);
    lv_obj_set_width(ui_blet2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_blet2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_blet2, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_blet2, "66666666666");
    lv_obj_add_flag(ui_blet2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_color(ui_blet2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_blet2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_blet2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Paboutb = lv_btn_create(ui_AboutP);
    lv_obj_set_width(ui_Paboutb, 60);
    lv_obj_set_height(ui_Paboutb, 60);
    lv_obj_set_x(ui_Paboutb, 0);
    lv_obj_set_y(ui_Paboutb, 120);
    lv_obj_set_align(ui_Paboutb, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Paboutb, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Paboutb, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Paboutb, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Paboutb, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Paboutb, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Paboutb, &ui_img_button_cancel_png, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_abtp = lv_obj_create(ui_Page_SAbout);
    lv_obj_set_width(ui_abtp, 288);
    lv_obj_set_height(ui_abtp, 60);
    lv_obj_set_x(ui_abtp, 0);
    lv_obj_set_y(ui_abtp, -179);
    lv_obj_set_align(ui_abtp, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_abtp, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_abtp, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_abtp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_abtp, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_abtp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_abtitle = lv_label_create(ui_abtp);
    lv_obj_set_width(ui_abtitle, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_abtitle, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_abtitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_abtitle, "About");
    lv_obj_set_style_text_color(ui_abtitle, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_abtitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_abtitle, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_aboutdn, ui_event_aboutdn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_aboutsv, ui_event_aboutsv, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_aboutsn, ui_event_aboutsn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_abouteui, ui_event_abouteui, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_aboutble, ui_event_aboutble, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Paboutb, ui_event_Paboutb, LV_EVENT_ALL, NULL);

}