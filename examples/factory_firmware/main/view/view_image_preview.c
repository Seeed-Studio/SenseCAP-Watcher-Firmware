#include "esp_log.h"
#include <mbedtls/base64.h>

#include "view_image_preview.h"

#define IMG_WIDTH 240
#define IMG_HEIGHT 240
#define IMG_240_240_BUF_SIZE 48*1024

#define RECTANGLE_COLOR lv_palette_main(LV_PALETTE_RED)

#define  IMAGE_INVOKED_BOXES_DISPLAY_ENABLE

static lv_img_dsc_t img_dsc = {
    .header.always_zero = 0,
    .header.w = IMG_WIDTH,
    .header.h = IMG_HEIGHT,
    .data_size = 0,
    .header.cf =LV_IMG_CF_RAW_ALPHA,
    .data = NULL,
};

lv_obj_t *ui_model_name = NULL;

static lv_obj_t *ui_image = NULL;
static lv_obj_t * ui_rectangle[IMAGE_INVOKED_BOXES];
static lv_obj_t * ui_class_name[IMAGE_INVOKED_BOXES];

static uint8_t *image_buf;

int g_model_id;
int view_image_preview_init(lv_obj_t *ui_screen)
{
    image_buf = malloc(IMG_240_240_BUF_SIZE);
    assert(image_buf);

    ui_image = lv_img_create(ui_screen);
    lv_obj_set_align(ui_image, LV_ALIGN_CENTER);

    ui_model_name = lv_label_create(ui_image);
    lv_obj_set_width( ui_model_name, LV_SIZE_CONTENT);
    lv_obj_set_height( ui_model_name, LV_SIZE_CONTENT); 
    lv_obj_set_x( ui_model_name, 0 );
    lv_obj_set_y( ui_model_name, 75 );
    lv_obj_set_align( ui_model_name, LV_ALIGN_CENTER );
    lv_label_set_text(ui_model_name, "Person Detection");
    // lv_obj_set_style_text_color(ui_model_name, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_color(ui_model_name, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_opa( ui_model_name, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_model_name, &lv_font_montserrat_26, LV_PART_MAIN| LV_STATE_DEFAULT);

#ifdef IMAGE_INVOKED_BOXES_DISPLAY_ENABLE
    for (size_t i = 0; i < IMAGE_INVOKED_BOXES; i++)
    {
        ui_rectangle[i]= lv_obj_create(ui_screen);
        lv_obj_add_flag( ui_rectangle[i], LV_OBJ_FLAG_HIDDEN);

        ui_class_name[i]= lv_label_create( ui_screen);
        lv_obj_set_width( ui_class_name[i], LV_SIZE_CONTENT);  /// 1
        lv_obj_set_height( ui_class_name[i], LV_SIZE_CONTENT);   /// 1
        lv_obj_set_style_text_font(ui_class_name[i], &lv_font_montserrat_26, LV_PART_MAIN| LV_STATE_DEFAULT);
        lv_obj_add_flag( ui_class_name[i], LV_OBJ_FLAG_HIDDEN);
    
    }
#endif
    return 0;
}

int view_image_preview_flush(struct view_data_image_invoke *p_invoke)
{
    int ret = 0;
    size_t output_len = 0;

    if( ui_image == NULL ) {
        return -1;
    }
    ret = mbedtls_base64_decode(image_buf, IMG_240_240_BUF_SIZE, &output_len, p_invoke->image.p_buf, p_invoke->image.len);    
    if( ret != 0 ) {
        ESP_LOGI("", "mbedtls_base64_decode failed: %d", ret);
        return ret;
    }

    img_dsc.data_size = output_len;
    img_dsc.data = image_buf;
    lv_img_set_src(ui_image, &img_dsc); 

#ifdef IMAGE_INVOKED_BOXES_DISPLAY_ENABLE
    for (size_t i = 0; i < IMAGE_INVOKED_BOXES; i++)
    {
        if( i <  p_invoke->boxes_cnt) {
            int x = 0;
            int y = 0;
            int w = 0; 
            int h = 0;

            x = p_invoke->boxes[i].x;
            y = p_invoke->boxes[i].y;
            w = p_invoke->boxes[i].w;
            h = p_invoke->boxes[i].h;
            
            x = x - w / 2;
            y = y - h / 2;

            if(x < 0){
                x = 0;
            }

            if (y < 0) {
                y = 0;
            }
            
            char *p_class_name="";
            lv_color_t color;
            switch (g_model_id)
            {
                case 1:{
                    p_class_name = "person";
                    color = lv_palette_main(LV_PALETTE_RED);
                    break;
                }
                case 2:{
                    p_class_name = "apple";
                    color = lv_palette_main(LV_PALETTE_RED);
                    break;
                }
                case 3:{
                    if( p_invoke->boxes[i].target == 0) {
                        p_class_name = "paper";
                        color = lv_palette_main(LV_PALETTE_RED);
                    }
                    else if(p_invoke->boxes[i].target == 1 ) {
                        p_class_name = "rock";
                        color = lv_palette_main(LV_PALETTE_YELLOW);
                    } else {
                        p_class_name = "scissors";
                        color = lv_palette_main(LV_PALETTE_GREEN);
                    }
                    break;
                }

                default:
                    break;
            }

            lv_obj_set_pos(ui_rectangle[i], x, y);
            lv_obj_set_size(ui_rectangle[i], w, h);
            lv_obj_set_style_border_color(ui_rectangle[i], color, 0);
            lv_obj_set_style_border_width(ui_rectangle[i], 4, 0);
            lv_obj_set_style_bg_opa(ui_rectangle[i], LV_OPA_TRANSP, 0);
            lv_obj_clear_flag(ui_rectangle[i], LV_OBJ_FLAG_HIDDEN);

            // name
            char buf1[32];
            lv_snprintf(buf1, sizeof(buf1),"%s:%d", p_class_name, p_invoke->boxes[i].score);

            lv_obj_set_pos(ui_class_name[i], x, (y-10)<0?0:(y-10));
            lv_label_set_text(ui_class_name[i], buf1);
            lv_obj_set_style_bg_color(ui_class_name[i], color, LV_PART_MAIN | LV_STATE_DEFAULT );
            lv_obj_set_style_bg_opa(ui_class_name[i], 255, LV_PART_MAIN| LV_STATE_DEFAULT);
            lv_obj_clear_flag(ui_class_name[i], LV_OBJ_FLAG_HIDDEN);

        } else {
            lv_obj_add_flag(ui_rectangle[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_class_name[i], LV_OBJ_FLAG_HIDDEN);

        }
    }
#endif
    return 0;
}