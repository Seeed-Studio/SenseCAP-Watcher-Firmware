#include "view_alarm.h"
#include "ui/ui.h"
#include "ui_manager/pm.h"
#include "ui_manager/event.h"
#include "esp_timer.h"
#include "data_defs.h"

#include <mbedtls/base64.h>
#include "esp_jpeg_dec.h"
#include "util.h"

uint8_t emoticon_disp_id = 0;
uint8_t view_alarm_status = 0;
lv_anim_t a;
lv_obj_t *ui_alarm_indicator;
lv_obj_t * ui_taskerrt2;
lv_obj_t * ui_task_error;

// view_alarm obj 
lv_obj_t * ui_viewavap;
lv_obj_t * ui_viewpbtn1;
lv_obj_t * ui_viewpt1;
lv_obj_t * ui_viewpbtn2;
lv_obj_t * ui_viewpt2;
lv_obj_t * ui_viewpbtn3;

extern uint8_t g_avarlive;

extern uint8_t wifi_page_id;
extern int g_dev_binded;
extern uint8_t g_guide_step;
extern uint8_t g_alarm_p;
extern uint8_t g_avalivjump;

static int16_t indicator_value = 0;
static lv_obj_t * ui_image = NULL;
static lv_obj_t * ui_Page_test;
static lv_obj_t * ui_taskerrt;
static lv_obj_t * ui_taskerrbtn;
static lv_obj_t * ui_taskerrbt;

static uint8_t *image_jpeg_buf = NULL;
static uint8_t *image_ram_buf = NULL;

static jpeg_dec_io_t *jpeg_io = NULL;
static jpeg_dec_header_info_t *out_info = NULL;
static jpeg_dec_handle_t jpeg_dec = NULL;

static lv_img_dsc_t img_dsc = {
    .header.always_zero = 0,
    .header.w = IMG_WIDTH,
    .header.h = IMG_HEIGHT,
    .data_size = IMG_RAM_BUF_SIZE,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

static void alarm_timer_callback(void *arg);
static const esp_timer_create_args_t alarm_timer_args = { .callback = &alarm_timer_callback, .name = "alarm_on" };
static esp_timer_handle_t alarm_timer;

static void alarm_timer_callback(void *arg)
{
    esp_event_post_to(app_event_loop_handle, VIEW_EVENT_BASE, VIEW_EVENT_ALARM_OFF, NULL, NULL, pdMS_TO_TICKS(10000));
}

static void set_angle(void *obj, int32_t v)
{
    lv_arc_set_value(obj, v);
}

static void alarm_timer_start(int s)
{
    if (esp_timer_is_active(alarm_timer))
    {
        esp_timer_stop(alarm_timer);
    }
    ESP_ERROR_CHECK(esp_timer_start_once(alarm_timer, (uint64_t)s * 1000000));
}

static void alarm_timer_stop()
{
    if (esp_timer_is_active(alarm_timer))
    {
        esp_timer_stop(alarm_timer);
    }
}

static int jpeg_decoder_init(void)
{
    esp_err_t ret = ESP_OK;
    jpeg_dec_config_t config = { .output_type = JPEG_RAW_TYPE_RGB565_BE, .rotate = JPEG_ROTATE_0D };
    
    jpeg_dec = jpeg_dec_open(&config);
    if (jpeg_dec == NULL) {
        return ESP_FAIL;
    }

    jpeg_io = heap_caps_malloc(sizeof(jpeg_dec_io_t), MALLOC_CAP_SPIRAM);
    if (jpeg_io == NULL) {
        jpeg_dec_close(jpeg_dec);
        return ESP_FAIL;
    }
    memset(jpeg_io, 0, sizeof(jpeg_dec_io_t));

    out_info = heap_caps_aligned_alloc(16, sizeof(jpeg_dec_header_info_t), MALLOC_CAP_SPIRAM);
    if (out_info == NULL) {
        heap_caps_free(jpeg_io);
        jpeg_dec_close(jpeg_dec);
        return ESP_FAIL;
    }
    memset(out_info, 0, sizeof(jpeg_dec_header_info_t));

    return ret;
}

static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    esp_err_t ret = ESP_OK;

    if (!jpeg_dec || !jpeg_io || !out_info) {
        return ESP_FAIL;
    }

    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        return ret;
    }

    jpeg_io->outbuf = output_buf;
    int inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + inbuf_consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    return ret;
}

int view_alarm_init(lv_obj_t *ui_screen)
{
    int ret = jpeg_decoder_init();
    if (ret != ESP_OK) {
        return ret;
    }

    image_jpeg_buf = psram_malloc(IMG_JPEG_BUF_SIZE);
    assert(image_jpeg_buf);

    //must be 16 byte aligned
    image_ram_buf = heap_caps_aligned_alloc(16, IMG_RAM_BUF_SIZE, MALLOC_CAP_SPIRAM);
    assert(image_ram_buf);

    ui_alarm_indicator = lv_arc_create(ui_screen);
    lv_obj_set_width(ui_alarm_indicator, 412);
    lv_obj_set_height(ui_alarm_indicator, 412);
    lv_obj_set_align(ui_alarm_indicator, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_alarm_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_alarm_indicator, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_value(ui_alarm_indicator, 0);
    lv_arc_set_rotation(ui_alarm_indicator, 270);
    lv_arc_set_bg_angles(ui_alarm_indicator, 0, 360);

    lv_obj_set_style_arc_color(ui_alarm_indicator, lv_color_hex(0x4040FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_alarm_indicator, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_alarm_indicator, 40, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_img_src(ui_alarm_indicator, &ui_img_gradient_png, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_alarm_indicator, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alarm_indicator, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_image = lv_img_create(ui_viewlivp2);
    lv_obj_set_align(ui_image, LV_ALIGN_CENTER);

    view_task_error_init();
    view_alarm_panel_init();

    ESP_ERROR_CHECK(esp_timer_create(&alarm_timer_args, &alarm_timer));

    return 0;
}


int view_alarm_on(struct tf_module_local_alarm_info *alarm_st)
{
    if((!g_dev_binded) && (g_guide_step != 3)){return 0;}

    if(lv_scr_act() == ui_Page_ViewAva)
    {
        if(view_alarm_status == 0)g_avalivjump = 0;
    }
    if(lv_scr_act() == ui_Page_ViewLive)
    {
        if(view_alarm_status == 0)g_avalivjump = 1;
    }

    alarm_timer_start(alarm_st->duration);
    if((lv_scr_act() != ui_Page_ViewAva) && (lv_scr_act() != ui_Page_ViewLive)){return 0;}
    // for switch avatar emoticon
    emoticon_disp_id = 1;
    // send focused event to call function
    if(lv_scr_act() == ui_Page_ViewAva)lv_event_send(ui_Page_ViewAva, LV_EVENT_SCREEN_LOADED, NULL);

    // turn the page to view live
    if ((lv_scr_act() != ui_Page_ViewLive) && (g_alarm_p == 0) && (g_avarlive == 0)){
        lv_group_focus_obj(ui_Page_ViewLive);
    }
    // clear alarm text
    lv_label_set_text(ui_viewtext, "");
    // record the page when the alarm is triggered
    // text display
    if (alarm_st->is_show_text)
    {
        struct tf_data_buf *alarm_text = &alarm_st->text;
        if (alarm_text->p_buf != NULL)
        {
            lv_label_set_text(ui_viewtext, (const char *)alarm_text->p_buf);
            tf_data_buf_free(alarm_text);
        }
    }
    // image display 
    if (alarm_st->is_show_img)
    {
        struct tf_data_image *alarm_img = &alarm_st->img;
        if (alarm_img->p_buf != NULL) {

            int ret = 0; 
            size_t output_len = 0;       
            ret = mbedtls_base64_decode(image_jpeg_buf, IMG_JPEG_BUF_SIZE, &output_len, alarm_img->p_buf, alarm_img->len);
            if (ret != 0 || output_len == 0)
            {
                ESP_LOGE("view", "Failed to decode base64: %d", ret);
                return ret;
            }

            ret = esp_jpeg_decoder_one_picture(image_jpeg_buf, output_len, image_ram_buf);
            if (ret != ESP_OK) {
                ESP_LOGE("view", "Failed to decode jpeg: %d", ret);
                return ret;
            }

            img_dsc.data = image_ram_buf;
            lv_img_set_src(ui_image, &img_dsc);
            lv_obj_clear_flag(ui_image, LV_OBJ_FLAG_HIDDEN);
            tf_data_image_free(&alarm_st->img);
        }
    }
    // initial indicator and state
    if(lv_scr_act() == ui_Page_ViewLive){
        indicator_value = 0;
        lv_arc_set_value(ui_alarm_indicator, indicator_value);
        lv_obj_clear_flag(ui_alarm_indicator, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_viewlivp2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(ui_image);
        lv_obj_move_foreground(ui_viewlivp2);

        // alarm indicator animation start
        lv_anim_init(&a);
        lv_anim_set_var(&a, ui_alarm_indicator);
        lv_anim_set_exec_cb(&a, set_angle);
        lv_anim_set_time(&a, (alarm_st->duration) * 1000);
        lv_anim_set_values(&a, 10, 100);
        lv_anim_start(&a);
    }
    view_alarm_status = 1;

    return ESP_OK;
}

void view_alarm_off(uint8_t task_down)
{    
    alarm_timer_stop();
    lv_obj_add_flag(ui_alarm_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_viewlivp2, LV_OBJ_FLAG_HIDDEN);
    // for switch avatar emoticon
    emoticon_disp_id = 0;
    if((lv_scr_act() != ui_Page_ViewAva) && (lv_scr_act() != ui_Page_ViewLive)){return ;}
    if(lv_scr_act() == ui_Page_ViewAva){
        lv_event_send(ui_Page_ViewAva, LV_EVENT_SCREEN_LOADED, NULL);
        lv_group_focus_obj(ui_Page_ViewAva);
    }
    if(lv_scr_act() == ui_Page_ViewLive){
        lv_group_focus_obj(ui_Page_ViewLive);
    }
    // if the page is avatar when the alarm is triggered, turn the page back when the alarm is off
    if(g_avalivjump == 0)
    {
        _ui_screen_change(&ui_Page_ViewAva, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_Page_ViewAva_screen_init);
        lv_group_focus_obj(ui_Page_ViewAva);
    }
    view_alarm_status = 0;
}

void view_task_error_init()
{
    ui_task_error = lv_obj_create(lv_layer_top());
    lv_obj_set_width(ui_task_error, 412);
    lv_obj_set_height(ui_task_error, 412);
    lv_obj_set_align(ui_task_error, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_task_error, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_task_error, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_task_error, 190, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_task_error, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_task_error, 230, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_task_error, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_task_error, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_taskerrt = lv_label_create(ui_task_error);
    lv_obj_set_width(ui_taskerrt, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_taskerrt, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_taskerrt, 0);
    lv_obj_set_y(ui_taskerrt, -104);
    lv_obj_set_align(ui_taskerrt, LV_ALIGN_CENTER);
    lv_label_set_text(ui_taskerrt, "TASK ERROR");
    lv_obj_set_style_text_color(ui_taskerrt, lv_color_hex(0xBE2C2C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_taskerrt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_taskerrt, &ui_font_fbold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_taskerrt2 = lv_label_create(ui_task_error);
    lv_obj_set_width(ui_taskerrt2, 289);
    lv_obj_set_height(ui_taskerrt2, 108);
    lv_obj_set_x(ui_taskerrt2, 0);
    lv_obj_set_y(ui_taskerrt2, -5);
    lv_obj_set_align(ui_taskerrt2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_taskerrt2, "[sensecraft alarm] failed to connect the next module");
    lv_obj_set_style_text_color(ui_taskerrt2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_taskerrt2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_taskerrt2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_taskerrt2, &ui_font_fontbold26, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_taskerrbtn = lv_btn_create(ui_task_error);
    lv_obj_set_width(ui_taskerrbtn, 150);
    lv_obj_set_height(ui_taskerrbtn, 60);
    lv_obj_set_x(ui_taskerrbtn, 0);
    lv_obj_set_y(ui_taskerrbtn, 110);
    lv_obj_set_align(ui_taskerrbtn, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_taskerrbtn, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_taskerrbtn, lv_color_hex(0xB80808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_taskerrbtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_taskerrbtn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_taskerrbtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_taskerrbt = lv_label_create(ui_taskerrbtn);
    lv_obj_set_width(ui_taskerrbt, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_taskerrbt, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_taskerrbt, LV_ALIGN_CENTER);
    lv_label_set_text(ui_taskerrbt, "End Task");
    lv_obj_set_style_text_color(ui_taskerrbt, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_taskerrbt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_taskerrbt, LV_TEXT_ALIGN_AUTO, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_taskerrbt, &ui_font_fbold24, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_taskerrbtn, taskerrc_cb, LV_EVENT_CLICKED, NULL);
}

void view_alarm_panel_init()
{
    ui_viewavap = lv_obj_create(lv_layer_top());
    lv_obj_set_width(ui_viewavap, 412);
    lv_obj_set_height(ui_viewavap, 412);
    lv_obj_set_align(ui_viewavap, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_viewavap, LV_OBJ_FLAG_HIDDEN);      /// Flags
    lv_obj_clear_flag(ui_viewavap, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_viewavap, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_viewavap, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_viewavap, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_viewavap, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_viewpbtn1 = lv_btn_create(ui_viewavap);
    lv_obj_set_width(ui_viewpbtn1, 270);
    lv_obj_set_height(ui_viewpbtn1, 80);
    lv_obj_set_x(ui_viewpbtn1, 0);
    lv_obj_set_y(ui_viewpbtn1, -90);
    lv_obj_set_align(ui_viewpbtn1, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_viewpbtn1, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_viewpbtn1, lv_color_hex(0x8FC31F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_viewpbtn1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_viewpbtn1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_viewpbtn1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_viewpt1 = lv_label_create(ui_viewpbtn1);
    lv_obj_set_width(ui_viewpt1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_viewpt1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_viewpt1, 1);
    lv_obj_set_y(ui_viewpt1, 0);
    lv_obj_set_align(ui_viewpt1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_viewpt1, "Main Menu");
    lv_obj_set_style_text_color(ui_viewpt1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_viewpt1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_viewpt1, &ui_font_font_bold, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_viewpbtn2 = lv_btn_create(ui_viewavap);
    lv_obj_set_width(ui_viewpbtn2, 270);
    lv_obj_set_height(ui_viewpbtn2, 80);
    lv_obj_set_x(ui_viewpbtn2, 0);
    lv_obj_set_y(ui_viewpbtn2, 10);
    lv_obj_set_align(ui_viewpbtn2, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_viewpbtn2, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_viewpbtn2, lv_color_hex(0xD54941), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_viewpbtn2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_viewpbtn2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_viewpbtn2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_viewpt2 = lv_label_create(ui_viewpbtn2);
    lv_obj_set_width(ui_viewpt2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_viewpt2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_viewpt2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_viewpt2, "End Task");
    lv_obj_set_style_text_color(ui_viewpt2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_viewpt2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_viewpt2, &ui_font_font_bold, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_viewpbtn3 = lv_btn_create(ui_viewavap);
    lv_obj_set_width(ui_viewpbtn3, 100);
    lv_obj_set_height(ui_viewpbtn3, 100);
    lv_obj_set_x(ui_viewpbtn3, 0);
    lv_obj_set_y(ui_viewpbtn3, 120);
    lv_obj_set_align(ui_viewpbtn3, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_viewpbtn3, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_viewpbtn3, lv_color_hex(0x151515), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_viewpbtn3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_viewpbtn3, &ui_img_setback_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_viewpbtn3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_viewpbtn3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_viewpbtn1, ui_event_alarm_panel, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_viewpbtn2, ui_event_alarm_panel, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_viewpbtn3, ui_event_alarm_panel, LV_EVENT_ALL, NULL);
}