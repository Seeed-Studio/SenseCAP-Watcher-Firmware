// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.0
// LVGL version: 8.3.6
// Project name: indoor_combo

#include "../ui.h"

void ui_screen_ha_ctrl_screen_init(void)
{
ui_screen_ha_ctrl = lv_obj_create(NULL);
lv_obj_clear_flag( ui_screen_ha_ctrl, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_button_panel_1 = lv_obj_create(ui_screen_ha_ctrl);
lv_obj_set_width( ui_button_panel_1, 150);
lv_obj_set_height( ui_button_panel_1, 40);
lv_obj_set_x( ui_button_panel_1, 0 );
lv_obj_set_y( ui_button_panel_1, -50 );
lv_obj_set_align( ui_button_panel_1, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_button_panel_1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_switch_1 = lv_switch_create(ui_button_panel_1);
lv_obj_set_width( ui_switch_1, 50);
lv_obj_set_height( ui_switch_1, 25);
lv_obj_set_x( ui_switch_1, 40 );
lv_obj_set_y( ui_switch_1, 0 );
lv_obj_set_align( ui_switch_1, LV_ALIGN_CENTER );

ui_object_set_themeable_style_property(ui_switch_1, LV_PART_INDICATOR| LV_STATE_CHECKED, LV_STYLE_BG_COLOR, _ui_theme_color_green);
ui_object_set_themeable_style_property(ui_switch_1, LV_PART_INDICATOR| LV_STATE_CHECKED, LV_STYLE_BG_OPA, _ui_theme_alpha_green);

ui_switch_label_1 = lv_label_create(ui_button_panel_1);
lv_obj_set_width( ui_switch_label_1, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_switch_label_1, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_switch_label_1, -30 );
lv_obj_set_y( ui_switch_label_1, 0 );
lv_obj_set_align( ui_switch_label_1, LV_ALIGN_CENTER );
lv_label_set_text(ui_switch_label_1,"Button 1");

ui_button_panel_2 = lv_obj_create(ui_screen_ha_ctrl);
lv_obj_set_width( ui_button_panel_2, 150);
lv_obj_set_height( ui_button_panel_2, 40);
lv_obj_set_align( ui_button_panel_2, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_button_panel_2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_switch_2 = lv_switch_create(ui_button_panel_2);
lv_obj_set_width( ui_switch_2, 50);
lv_obj_set_height( ui_switch_2, 25);
lv_obj_set_x( ui_switch_2, 40 );
lv_obj_set_y( ui_switch_2, 0 );
lv_obj_set_align( ui_switch_2, LV_ALIGN_CENTER );

ui_object_set_themeable_style_property(ui_switch_2, LV_PART_INDICATOR| LV_STATE_CHECKED, LV_STYLE_BG_COLOR, _ui_theme_color_green);
ui_object_set_themeable_style_property(ui_switch_2, LV_PART_INDICATOR| LV_STATE_CHECKED, LV_STYLE_BG_OPA, _ui_theme_alpha_green);

ui_switch_label_2 = lv_label_create(ui_button_panel_2);
lv_obj_set_width( ui_switch_label_2, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_switch_label_2, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_switch_label_2, -30 );
lv_obj_set_y( ui_switch_label_2, 0 );
lv_obj_set_align( ui_switch_label_2, LV_ALIGN_CENTER );
lv_label_set_text(ui_switch_label_2,"Button 2");

ui_button_panel_3 = lv_obj_create(ui_screen_ha_ctrl);
lv_obj_set_width( ui_button_panel_3, 150);
lv_obj_set_height( ui_button_panel_3, 40);
lv_obj_set_x( ui_button_panel_3, 0 );
lv_obj_set_y( ui_button_panel_3, 50 );
lv_obj_set_align( ui_button_panel_3, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_button_panel_3, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_switch_3 = lv_switch_create(ui_button_panel_3);
lv_obj_set_width( ui_switch_3, 50);
lv_obj_set_height( ui_switch_3, 25);
lv_obj_set_x( ui_switch_3, 40 );
lv_obj_set_y( ui_switch_3, 0 );
lv_obj_set_align( ui_switch_3, LV_ALIGN_CENTER );

ui_object_set_themeable_style_property(ui_switch_3, LV_PART_INDICATOR| LV_STATE_CHECKED, LV_STYLE_BG_COLOR, _ui_theme_color_green);
ui_object_set_themeable_style_property(ui_switch_3, LV_PART_INDICATOR| LV_STATE_CHECKED, LV_STYLE_BG_OPA, _ui_theme_alpha_green);

ui_switch_label_3 = lv_label_create(ui_button_panel_3);
lv_obj_set_width( ui_switch_label_3, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_switch_label_3, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_switch_label_3, -30 );
lv_obj_set_y( ui_switch_label_3, 0 );
lv_obj_set_align( ui_switch_label_3, LV_ALIGN_CENTER );
lv_label_set_text(ui_switch_label_3,"Button 3");

lv_obj_add_event_cb(ui_screen_ha_ctrl, ui_event_screen_ha_ctrl, LV_EVENT_ALL, NULL);

}