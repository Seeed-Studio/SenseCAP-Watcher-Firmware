#include "view.h"
#include "view_image_preview.h"
#include "view_alarm.h"
#include "sensecap-watcher.h"

#include "util.h"
#include "ui/ui_helpers.h"
#include <time.h>
#include "app_device_info.h"
#include "app_png.h"
#include "ui_manager/pm.h"
#include "ui_manager/animation.h"
#include "ui_manager/event.h"

#define PNG_IMG_NUMS 24

static const char *TAG = "view";

uint8_t wifi_page_id;
lv_obj_t * view_show_img;
uint8_t shutdown_state = 0;
static int PNG_LOADING_COUNT = 0;
static uint8_t battery_per = 0;
static bool battery_timer_toggle = 0;
static int battery_flash_count = 0;
static lv_obj_t * mbox1;
extern uint8_t g_taskdown;
extern uint8_t g_swipeid; // 0 for shutdown, 1 for factoryreset
extern int g_dev_binded;
extern uint8_t g_avarlive;
extern uint8_t g_tasktype;
extern uint8_t g_backpage;
extern lv_obj_t * ui_taskerrt2;
extern lv_obj_t * ui_task_error;

extern lv_img_dsc_t *g_detect_img_dsc[MAX_IMAGES];
extern lv_img_dsc_t *g_speak_img_dsc[MAX_IMAGES];
extern lv_img_dsc_t *g_listen_img_dsc[MAX_IMAGES];
extern lv_img_dsc_t *g_anaylze_img_dsc[MAX_IMAGES];
extern lv_img_dsc_t *g_standby_img_dsc[MAX_IMAGES];
extern lv_img_dsc_t *g_greet_img_dsc[MAX_IMAGES];
extern lv_img_dsc_t *g_detected_img_dsc[MAX_IMAGES];

// view_alarm obj extern
extern lv_obj_t * ui_viewavap;
extern lv_obj_t * ui_viewpbtn1;
extern lv_obj_t * ui_viewpt1;
extern lv_obj_t * ui_viewpbtn2;
extern lv_obj_t * ui_viewpt2;
extern lv_obj_t * ui_viewpbtn3;

extern int g_detect_image_count;
extern int g_speak_image_count;
extern int g_listen_image_count;
extern int g_analyze_image_count;
extern int g_standby_image_count;
extern int g_greet_image_count;
extern int g_detected_image_count;

static void update_ai_ota_progress(int percentage)
{
    lv_arc_set_value(ui_waitarc, percentage);
    char percentage_str[4];
    sprintf(percentage_str, "%d", percentage);
    lv_label_set_text(ui_otatper, percentage_str);
    ESP_LOGI(TAG, "OTA progress updated: %d%%", percentage);
}

static void update_ota_progress(int percentage)
{
    lv_arc_set_value(ui_otaarc, percentage);
}

static void toggle_image_visibility(lv_timer_t *timer)
{
    if(battery_timer_toggle){
        lv_obj_add_flag(ui_Image2, LV_OBJ_FLAG_HIDDEN);
    }else{
        lv_obj_clear_flag(ui_Image2, LV_OBJ_FLAG_HIDDEN);
    }
    battery_timer_toggle = !battery_timer_toggle;

    battery_flash_count++;

    if (battery_flash_count >= 4) {
        lv_timer_del(timer);
        esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_SHUTDOWN, NULL, 0, pdMS_TO_TICKS(10000));
    }
}

static void __view_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    lvgl_port_lock(0);
    if(base == VIEW_EVENT_BASE){
        switch (id)
        {
            case VIEW_EVENT_SCREEN_START: {
                _ui_screen_change(&ui_Page_Vir, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_Vir_screen_init);
                break;
            }

            case VIEW_EVENT_PNG_LOADING:{
                PNG_LOADING_COUNT++;
                int progress_percentage = (PNG_LOADING_COUNT * 100) / PNG_IMG_NUMS;
                if(progress_percentage <= 100){
                    lv_arc_set_value(ui_Arc1, progress_percentage);
                    static char load_per[5];
                    sprintf(load_per, "%d%%", progress_percentage);
                    lv_label_set_text(ui_loadpert, load_per);
                }
                if(progress_percentage>=50)
                {
                    lv_event_send(ui_Page_loading, LV_EVENT_SCREEN_LOADED, NULL);
                }
                break;
            }

            case VIEW_EVENT_INFO_OBTAIN:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_INFO_OBTAIN");
                view_info_obtain();
                break;
            }

            case VIEW_EVENT_USAGE_GUIDE_SWITCH:
            {
                ESP_LOGI(TAG, "event: VIEW_EVENT_USAGE_GUIDE_SWITCH");
                int *usage_guide_st = (int *)event_data;
                g_dev_binded = (*usage_guide_st);
                // ESP_LOGI(TAG, "g_dev_binded : %d", g_dev_binded);
                break;
            }

            case VIEW_EVENT_BATTERY_ST:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_BATTERY_ST");
                struct view_data_device_status * bat_st = (struct view_data_device_status *)event_data;
                ESP_LOGI(TAG, "battery_percentage: %d", bat_st->battery_per);
                static char load_per[5];
                sprintf(load_per, "%d", bat_st->battery_per);
                lv_label_set_text(ui_btpert, load_per);
                break;
            }

            case VIEW_EVENT_BAT_DRAIN_SHUTDOWN:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_BAT_DRAIN_SHUTDOWN");
                _ui_screen_change(&ui_Page_Battery, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_Battery_screen_init);
                lv_timer_t *timer = lv_timer_create(toggle_image_visibility, 500, NULL);
                
                break;
            }

            case VIEW_EVENT_CHARGE_ST:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_CHARGE_ST");
                uint8_t is_charging = *(uint8_t *)event_data;
                ESP_LOGI(TAG, "charging state changed: %d", is_charging);
                lv_obj_add_flag(ui_btpert, LV_OBJ_FLAG_HIDDEN);
                if(is_charging == 1)
                {
                    shutdown_state = 0;
                    if(g_swipeid==0)
                    {
                        lv_label_set_text(ui_setdownt, "Reboot");
                        lv_label_set_text(ui_sptext, "Swipe to reboot");
                        lv_label_set_text(ui_sptitle, "Reboot");
                    }
                    lv_obj_add_flag(ui_btpert, LV_OBJ_FLAG_HIDDEN);
                    lv_img_set_src(ui_mainb, &ui_img_battery_charging_png);
                }else if(is_charging == 0){
                    shutdown_state = 1;
                    if(g_swipeid==0)
                    {
                        lv_label_set_text(ui_setdownt, "Shutdown");
                        lv_label_set_text(ui_sptext, "Swipe to shut down");
                        lv_label_set_text(ui_sptitle, "Shut down");
                    }
                    lv_obj_clear_flag(ui_btpert, LV_OBJ_FLAG_HIDDEN);
                    lv_img_set_src(ui_mainb, &ui_img_battery_frame_png);
                }
                break;
            } 

            case VIEW_EVENT_TIME: {
                ESP_LOGI(TAG, "event: VIEW_EVENT_TIME");
                bool time_format_24 = true;
                if( event_data) {
                    time_format_24 = *( bool *)event_data;
                } 
                
                time_t now = 0;
                struct tm timeinfo = { 0 };

                time(&now);
                localtime_r(&now, &timeinfo);
                int hour = timeinfo.tm_hour;

                if( ! time_format_24 ) {
                    if( hour>=13 && hour<=23) {
                        hour = hour-12;
                    }
                }
                char buf1[32];
                lv_snprintf(buf1, sizeof(buf1), "%02d:%02d",hour, timeinfo.tm_min);
                lv_label_set_text(ui_maintime, buf1);
                break;
            }

            case VIEW_EVENT_WIFI_ST: {
                ESP_LOGI(TAG, "event: VIEW_EVENT_WIFI_ST");
                struct view_data_wifi_st *p_st = ( struct view_data_wifi_st *)event_data;
                uint8_t *p_src =NULL;
                if ( p_st->past_connected)
                {
                    wifi_page_id = 1;
                }else{
                    wifi_page_id = 0;
                }
                if ( p_st->is_network ) {
                    switch (wifi_rssi_level_get( p_st->rssi )) {
                        case 1:
                            p_src = &ui_img_wifi_0_png;
                            break;
                        case 2:
                            p_src = &ui_img_wifi_1_png;
                            break;
                        case 3:
                            p_src = &ui_img_wifi_2_png;
                        case 4:
                            p_src = &ui_img_wifi_3_png;
                            break;
                        default:
                            break;
                    }
        
                } else if( p_st->is_connected ) {
                    p_src = &ui_img_wifi_abnormal_png;
                } else {
                    p_src = &ui_img_no_wifi_png;
                }
                lv_img_set_src(ui_mainwifi , (void *)p_src);
                break;
            }

            case VIEW_EVENT_WIFI_CONFIG_SYNC:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_WIFI_CONFIG_SYNC");
                int * wifi_config_sync = (int*)event_data;
                if(lv_scr_act() != ui_Page_Wifi)_ui_screen_change(&ui_Page_Wifi, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_Wifi_screen_init);
                if(* wifi_config_sync == 0)
                {
                    waitForWifi();
                }else if(* wifi_config_sync== 1)
                {
                    waitForBinding();
                }else if(*wifi_config_sync == 2)
                {
                    waitForAddDev();
                }else if(* wifi_config_sync == 3)
                {
                    bindFinish();
                    wifi_page_id = 1;
                    lv_obj_add_flag(ui_virp, LV_OBJ_FLAG_HIDDEN);
                    _ui_screen_change(&ui_Page_Vir, LV_SCR_LOAD_ANIM_FADE_ON, 100, 3000, &ui_Page_Vir_screen_init);
                }else if(* wifi_config_sync == 4)
                {
                    wifiConnectFailed();
                }else if (* wifi_config_sync == 5)
                {
                    lv_pm_open_page(g_main, &group_page_set, PM_ADD_OBJS_TO_GROUP, &ui_Page_Set, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_Set_screen_init);
                }
                break;
            }

            case VIEW_EVENT_BLE_STATUS:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_BLE_STATUS");
                bool ble_connect_status = false;
                ble_connect_status = *(bool *)event_data;
                if(ble_connect_status)
                {
                    lv_obj_set_style_img_recolor(ui_mainble, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                else{
                    lv_obj_set_style_img_recolor(ui_mainble, lv_color_hex(0x171515), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                break;
            }

            case VIEW_EVENT_BRIGHTNESS:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_BRIGHTNESS");
                uint8_t *bri_st = (uint8_t *)event_data;
                int32_t bri_value = (int32_t)(*bri_st);
                lv_slider_set_value(ui_bslider, bri_value, LV_ANIM_OFF);
                                
                break;
            }

            case VIEW_EVENT_RGB_SWITCH:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_RGB_SWITCH");
                int * rgb_st = (int *)event_data;
                if((*rgb_st))
                {
                    lv_obj_add_state(ui_setrgbsw, LV_STATE_CHECKED);
                }else{
                    lv_obj_clear_state(ui_setrgbsw, LV_STATE_CHECKED);
                }

                break;
            }

            case VIEW_EVENT_SOUND:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_SOUND");
                uint8_t * vol_st = (uint8_t *)event_data;
                int32_t vol_value = (int32_t)(*vol_st);
                lv_slider_set_value(ui_vslider, (int32_t *)vol_value, LV_ANIM_OFF);
                
                break;
            }

            case VIEW_EVENT_ALARM_ON:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_ALARM_ON");
                struct tf_module_local_alarm_info *alarm_st = (struct tf_module_local_alarm_info *)event_data;
                             
                view_alarm_on(alarm_st);

                break;
            }

            case VIEW_EVENT_ALARM_OFF:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_ALARM_OFF");
                uint8_t * task_st = (uint8_t *)event_data;
                view_alarm_off(task_st);
                break;
            }

            case VIEW_EVENT_TASK_FLOW_START_CURRENT_TASK:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_TASK_FLOW_START_CURRENT_TASK");
                g_tasktype = 1;
                g_taskdown = 0;
                g_backpage = 1;
                lv_obj_add_flag(ui_task_error, LV_OBJ_FLAG_HIDDEN);
                _ui_screen_change(&ui_Page_CurTask3, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_CurTask3_screen_init);
                break;
            }

            case VIEW_EVENT_TASK_FLOW_STOP:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_TASK_FLOW_STOP");
                lv_obj_add_flag(ui_viewavap, LV_OBJ_FLAG_HIDDEN);
                // event_post_to
                g_taskdown = 1;
                esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_ALARM_OFF, &g_taskdown, sizeof(uint8_t), pdMS_TO_TICKS(10000));
                if(g_tasktype == 0)
                {
                    lv_pm_open_page(g_main, &group_page_template, PM_ADD_OBJS_TO_GROUP, &ui_Page_LocTask, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_LocTask_screen_init);
                }else{
                    lv_pm_open_page(g_main, &group_page_main, PM_ADD_OBJS_TO_GROUP, &ui_Page_main, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_main_screen_init);
                    lv_group_focus_obj(ui_mainbtn2);
                }
                break;
            }

            case VIEW_EVENT_AI_CAMERA_READY:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_AI_CAMERA_READY");
                if((lv_scr_act() != ui_Page_ViewAva) && (lv_scr_act() != ui_Page_ViewLive) && (lv_scr_act() != ui_Page_CurTask2) && (lv_scr_act() != ui_Page_CurTask3))
                {
                    break;
                }
                if(g_avarlive == 0)
                {
                    if(g_dev_binded)
                    {
                        if(lv_scr_act() != ui_Page_ViewAva)lv_pm_open_page(g_main, &group_page_view, PM_ADD_OBJS_TO_GROUP, &ui_Page_ViewAva, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_ViewAva_screen_init);
                    }else{
                        _ui_screen_change(&ui_Page_flag, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_flag_screen_init);
                    }
                }else if(g_avarlive == 1)
                {
                    if(g_dev_binded)
                    {
                        if(lv_scr_act() != ui_Page_ViewLive)lv_pm_open_page(g_main, &group_page_view, PM_ADD_OBJS_TO_GROUP, &ui_Page_ViewLive, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_ViewLive_screen_init);
                    }else{
                        _ui_screen_change(&ui_Page_flag, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_flag_screen_init);
                    }
                }
                break;
            }

            case VIEW_EVENT_OTA_STATUS:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_OTA_STATUS");
                if(lv_scr_act() != ui_Page_OTA)
                {
                    lv_group_remove_all_objs(g_main);
                    _ui_screen_change(&ui_Page_OTA, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_OTA_screen_init);
                }
                struct view_data_ota_status * ota_st = (struct view_data_ota_status *)event_data;
                ESP_LOGI(TAG, "VIEW_EVENT_OTA_STATUS: %d", ota_st->status);
                if(ota_st->status == 1)
                {
                    update_ota_progress(ota_st->percentage);
                    lv_label_set_text(ui_otatext, "Updating\nFirmware");
                    lv_obj_add_flag(ui_otaback, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_otaicon, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_otaspinner, LV_OBJ_FLAG_HIDDEN);
                }else if (ota_st->status == 2)
                {
                    ESP_LOGI(TAG, "OTA download succeeded");
                    lv_label_set_text(ui_otatext, "Update\nSuccessful");
                    lv_obj_clear_flag(ui_otaicon, LV_OBJ_FLAG_HIDDEN);
                    lv_img_set_src(ui_otaicon, &ui_img_wifiok_png);
                    lv_obj_add_flag(ui_otaspinner, LV_OBJ_FLAG_HIDDEN);
                }else if (ota_st->status == 3){
                    ESP_LOGE(TAG, "OTA download failed, error code: %d", ota_st->err_code);
                    lv_label_set_text(ui_otatext, "Update Failed");
                    lv_obj_clear_flag(ui_otaicon, LV_OBJ_FLAG_HIDDEN);
                    lv_img_set_src(ui_otaicon, &ui_img_error_png);
                    lv_obj_add_flag(ui_otaspinner, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_otaback, LV_OBJ_FLAG_HIDDEN);
                }
                break;
            }

            case VIEW_EVENT_TASK_FLOW_ERROR:{
                ESP_LOGI(TAG, "event: VIEW_EVENT_TASK_FLOW_ERROR");
                const char* error_msg = (const char*)event_data;
                lv_obj_clear_flag(ui_task_error, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_taskerrt2, error_msg);
                break;
            }
            default:
                break;
        }
    }
    else if(base == CTRL_EVENT_BASE){
        switch (id)
        {
            case CTRL_EVENT_OTA_AI_MODEL:{
                ESP_LOGI(TAG, "event: CTRL_EVENT_OTA_AI_MODEL");
                if(g_taskdown == 0) // if the task is running
                {
                    if(lv_scr_act() != ui_Page_CurTask2)
                    {
                        lv_group_remove_all_objs(g_main);
                        _ui_screen_change(&ui_Page_CurTask2, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_CurTask2_screen_init);
                    }
                    struct view_data_ota_status * ota_st = (struct view_data_ota_status *)event_data;
                    lv_obj_add_flag(ui_otaicon, LV_OBJ_FLAG_HIDDEN);
                    if(ota_st->status == 0)
                    {
                        ESP_LOGI(TAG, "OTA download succeeded");
                    }else if (ota_st->status == 1)
                    {
                        lv_obj_clear_flag(ui_otaspinner, LV_OBJ_FLAG_HIDDEN);
                        update_ai_ota_progress(ota_st->percentage);
                    }else{
                        lv_label_set_text(ui_otatext, "Update Failed");
                        lv_img_set_src(ui_otaicon, &ui_img_error_png);
                        lv_obj_add_flag(ui_otaspinner, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_clear_flag(ui_otaicon, LV_OBJ_FLAG_HIDDEN);
                        ESP_LOGE(TAG, "OTA download failed, error code: %d", ota_st->err_code);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
    lvgl_port_unlock();
}

int view_init(void)
{
    static uint8_t bat_per;
    static bool    is_charging;
    bat_per = bsp_battery_get_percent();
    is_charging = (uint8_t)(bsp_exp_io_get_level(BSP_PWR_VBUS_IN_DET) == 0);

    lvgl_port_lock(0);
    ui_init();
    lv_pm_init();
    view_alarm_init(lv_layer_top());
    view_image_preview_init(ui_Page_ViewLive);
    lvgl_port_unlock();

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_SCREEN_START, 
                                                            __view_event_handler, NULL, NULL)); 
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_PNG_LOADING, 
                                                            __view_event_handler, NULL, NULL)); 

    // ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
    //                                                         VIEW_EVENT_BASE, VIEW_EVENT_USAGE_GUIDE_SWITCH, 
    //                                                         __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_TIME, 
                                                            __view_event_handler, NULL, NULL)); 

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_BRIGHTNESS, 
                                                            __view_event_handler, NULL, NULL));  

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_RGB_SWITCH, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_SOUND, 
                                                            __view_event_handler, NULL, NULL)); 
                                                            
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_SOFTWARE_VERSION_CODE, 
                                                            __view_event_handler, NULL, NULL));   

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_HIMAX_SOFTWARE_VERSION_CODE, 
                                                            __view_event_handler, NULL, NULL)); 
                                                            
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_BLE_STATUS, 
                                                            __view_event_handler, NULL, NULL));                                                                                                                 

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST, 
                                                            __view_event_handler, NULL, NULL)); 
                                                            
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_WIFI_CONFIG_SYNC, 
                                                            __view_event_handler, NULL, NULL)); 

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_ALARM_ON, 
                                                            __view_event_handler, NULL, NULL));   

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_ALARM_OFF, 
                                                            __view_event_handler, NULL, NULL));  
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_BATTERY_ST, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_BAT_DRAIN_SHUTDOWN, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_CHARGE_ST, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            CTRL_EVENT_BASE, CTRL_EVENT_OTA_AI_MODEL, 
                                                            __view_event_handler, NULL, NULL)); 
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_TASK_FLOW_START_CURRENT_TASK, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_TASK_FLOW_STOP, 
                                                            __view_event_handler, NULL, NULL));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_OTA_STATUS, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_TASK_FLOW_ERROR, 
                                                            __view_event_handler, NULL, NULL)); 

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_AI_CAMERA_READY, 
                                                            __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop_handle, 
                                                            VIEW_EVENT_BASE, VIEW_EVENT_INFO_OBTAIN, 
                                                            __view_event_handler, NULL, NULL));

    if((bat_per < 1) && (! is_charging))
    {
        lv_disp_load_scr(ui_Page_Battery);
        vTaskDelay(pdMS_TO_TICKS(200));
        BSP_ERROR_CHECK_RETURN_ERR(bsp_lcd_brightness_set(50));

        ESP_LOGI(TAG, "Battery too low, wait for charging");
        while(1) {
            is_charging = (uint8_t)(bsp_exp_io_get_level(BSP_PWR_VBUS_IN_DET) == 0);
            if ( is_charging ) {
                ESP_LOGI(TAG, "Charging, exit low battery page");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    lv_disp_load_scr(ui____initial_actions0);
    lv_disp_load_scr(ui_Page_Start);

    vTaskDelay(pdMS_TO_TICKS(200));
    BSP_ERROR_CHECK_RETURN_ERR(bsp_lcd_brightness_set(50));

    read_and_store_selected_pngs("greeting", g_greet_img_dsc, &g_greet_image_count);
    read_and_store_selected_pngs("detecting", g_detect_img_dsc, &g_detect_image_count);
    read_and_store_selected_pngs("detected", g_detected_img_dsc, &g_detected_image_count);
    read_and_store_selected_pngs("speaking", g_speak_img_dsc, &g_speak_image_count);
    read_and_store_selected_pngs("listening", g_listen_img_dsc, &g_listen_image_count);
    read_and_store_selected_pngs("analyzing", g_anaylze_img_dsc, &g_analyze_image_count);
    read_and_store_selected_pngs("standby", g_standby_img_dsc, &g_standby_image_count);

    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_SCREEN_START, NULL, 0, pdMS_TO_TICKS(10000));
                    

    return 0;
}

void view_render_black(void)
{
    lvgl_port_lock(0);
    view_image_black_flush();
    lvgl_port_unlock();
}
