#include "animation.h"
#include "ui/ui.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "animation_event";

void main_scroll_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);

    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_y_center = lv_area_get_height(&cont_a) / 2;
    // ESP_LOGI(TAG, "cont_y_center: %d", cont_y_center);

    // int32_t r = lv_obj_get_height(cont);
    int32_t r = 250;
    uint32_t i;
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);

    for (i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);

        int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

        int32_t diff_y = child_y_center - cont_y_center;
        diff_y = LV_ABS(diff_y);

        /*Get the x of diff_y on a circle.*/
        int32_t x;
        /*If diff_y is out of the circle use the last point of the circle (the radius)*/
        if (diff_y >= r)
        {
            x = -r;
        }
        else
        {
            /*Use Pythagoras theorem to get x from radius and y*/
            uint32_t x_sqr = r * r - diff_y * diff_y;
            lv_sqrt_res_t res;
            lv_sqrt(x_sqr, &res, 0x8000); /*Use lvgl's built in sqrt root function*/
            x = -(r - res.i);
        }
        /*Translate the item by the calculated X coordinate*/
        lv_obj_set_style_translate_x(child, x, 0);
        lv_obj_set_style_opa(child, 250 + x + x, 0);
        
        // lv_obj_set_size(lv_obj_get_child(child, 0), x+80, x+80);
        // ESP_LOGI(TAG, "The value is: %" PRId32 "\n", x);
    }
}

void menu_scroll_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);

    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_x_center = lv_area_get_width(&cont_a) / 2; // Change to use the center point of the width
    // ESP_LOGI(TAG, "cont_x_center: %d", cont_x_center);

    // int32_t r = lv_obj_get_height(cont); // Change to use the width as the radius
    int32_t r = 140;

    uint32_t i;
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);

    for (i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);

        int32_t child_x_center = child_a.x1 + lv_area_get_width(&child_a) / 2; // Change to calculate the center point of the width

        int32_t diff_x = child_x_center - cont_x_center; // Change to calculate the horizontal difference
        diff_x = LV_ABS(diff_x);

        /*Get the y of diff_x on a circle.*/
        int32_t y;
        /*If diff_x is out of the circle use the last point of the circle (the radius).*/
        if (diff_x >= r)
        {
            y = -r; // Change to use a negative value for y on the lower semicircle
        }
        else
        {
            /*Use Pythagoras theorem to get y from radius and x.*/
            uint32_t y_sqr = r * r - diff_x * diff_x;
            lv_sqrt_res_t res;
            lv_sqrt(y_sqr, &res, 0x8000); // Use the built-in sqrt function from lvgl
            y = -(r - res.i);             // Change to use a negative value for y on the lower semicircle
        }
        /*Translate the item by the calculated Y coordinate.*/
        lv_obj_set_style_translate_y(child, y, 0);
        // ESP_LOGI(TAG, "The value is: %" PRId32 "\n", y);
    }
}

void set_scroll_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_y_center = lv_area_get_height(&cont_a) / 2;
    // ESP_LOGI(TAG, "cont_y_center: %d", cont_y_center);

    // int32_t r = lv_obj_get_height(cont);
    int32_t r = 412;
    uint32_t i;
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);

    for (i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);

        int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

        int32_t diff_y = child_y_center - cont_y_center;
        diff_y = LV_ABS(diff_y);

        /*Get the x of diff_y on a circle.*/
        int32_t x;
        /*If diff_y is out of the circle use the last point of the circle (the radius)*/
        if (diff_y >= r)
        {
            x = r;
        }
        else
        {
            /*Use Pythagoras theorem to get x from radius and y*/
            uint32_t x_sqr = r * r - diff_y * diff_y;
            lv_sqrt_res_t res;
            lv_sqrt(x_sqr, &res, 0x8000); /*Use lvgl's built in sqrt root function*/
            x = r - res.i;
        }
        /*Translate the item by the calculated X coordinate*/
        lv_obj_set_style_translate_x(child, x, 0);
    }
}


void scroll_anim_enable()
{
    // Page_main_scroll
    lv_obj_set_scroll_snap_y(ui_mainlist, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_dir(ui_mainlist, LV_DIR_VER);

    lv_obj_add_event_cb(ui_mainlist, main_scroll_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(ui_mainlist, 0), LV_ANIM_OFF);
    lv_event_send(ui_mainlist, LV_EVENT_SCROLL, NULL);

    // Page_menu_scroll
    lv_obj_set_scroll_snap_x(ui_menulist, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_dir(ui_menulist, LV_DIR_HOR);

    lv_obj_add_event_cb(ui_menulist, menu_scroll_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(ui_menulist, 0), LV_ANIM_OFF);
    lv_event_send(ui_menulist, LV_EVENT_SCROLL, NULL);

    // Page_set_scroll
    lv_obj_set_scroll_snap_y(ui_Set_panel, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_dir(ui_Set_panel, LV_DIR_VER);

    lv_obj_add_event_cb(ui_Set_panel, set_scroll_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(ui_Set_panel, 0), LV_ANIM_OFF);
    lv_event_send(ui_Set_panel, LV_EVENT_SCROLL, NULL);
}