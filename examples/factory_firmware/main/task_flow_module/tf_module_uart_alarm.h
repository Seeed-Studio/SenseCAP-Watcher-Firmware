/**
 * UART Alarm Function Module
 * Author: Jack Shao <jack.shao@seeed.cc>
 * Copyright Seeed Tech. Co., Ltd. 2024
*/
#pragma once
#include "esp_err.h"
#include "driver/uart.h"
#include "tf_module.h"
#include "tf_module_data_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TF_MODULE_UART_ALARM_NAME "uart alarm"
#define TF_MODULE_UART_ALARM_VERSION "1.0.0"
#define TF_MODULE_UART_ALARM_DESC "uart alarm function module"

/**
 * This function module take the input data from its parent module, then build a packet 
 * in specified format and output this packet via UART on the back of the Watcher.
*/

#define PKT_MAGIC_HEADER        "SEEED"

/**
 * The packet structure of binary output
 * 
 * +------------------+----------------+------------+---------------+-----------+-----------------+-------------+
 * | PKT_MAGIC_HEADER | Prompt Str Len | Prompt Str | Big Image Len | Big Image | Small Image Len | Small Image |
 * +------------------+----------------+------------+---------------+-----------+-----------------+-------------+ + 
 * | "SEEED"(5bytes)  | 2bytes         | X bytes    | 2bytes        | Y bytes   | 2bytes          | Z bytes     |
 * +------------------+----------------+------------+---------------+-----------+-----------------+-------------+
 * +-------------+------------------------+---------------+---------------+---------------+
 * | Boxes Count |         Box 1          |     Box 2     |      ...      |     Box N     |
 * +-------------+------------------------+---------------+---------------+---------------+
 * | 2bytes      | Box Structure(10bytes) | Box Structure | Box Structure | Box Structure |
 * +-------------+------------------------+---------------+---------------+---------------+
 *               /                        \
 * /------------/                          \------------------------------\
 * +----------+----------+----------+----------+---------+-----------------+
 * |    x     |    y     |    w     |    h     |  score  | target class id |
 * +----------+----------+----------+----------+---------+-----------------+
 * | 2bytes   | 2bytes   | 2bytes   | 2bytes   | 1byte   | 1byte           |
 * | uint16_t | uint16_t | uint16_t | uint16_t | uint8_t | uint8_t         |
 * +----------+----------+----------+----------+---------+-----------------+
 * 
 * This is the full packet with all fields enabled.
 * 
 * - Big Image: 640 * 480 image, base64 encoded JPG image, without boxes of detected objects.
 * - Small Image: 240 * 240 image, base64 encoded JPG image, with boxes drawn for detected objects.
 * - Box: A area which holds the detected object, with its coordinates and score.
 * 
 * Please note, Big Image and Small Image buffer has no string terminator '\0'.
 * 
 * Some of the fields can be controlled by configuration of the function module, see the comments for 
 * `tf_module_uart_alarm_t` below. 
 * 
 * `include_big_image`, `include_small_image` and `include_boxes` are disabled by default. So if you don't apply
 * any configuration to the function module, the default output packet will only include the following fields:
 * PKT_MAGIC_HEADER + Prompt Str Len + Prompt Str
 * 
 * Another example, if the `include_big_image` configuration is enabled, the `Big Image Len` and `Big Image` fields
 * will be added to the output packet.
 * PKT_MAGIC_HEADER + Prompt Str Len + Prompt Str + Big Image Len + Big Image
 * 
*/

/**
 * The packet structure of the JSON output
 * 
 * +------------------+--------+-------------+
 * | PKT_MAGIC_HEADER |  Len   |    JSON     |
 * +------------------+--------+-------------+
 * | "SEEED"(5bytes)  | 2bytes | `Len` bytes |
 * +------------------+--------+-------------+
 * 
 * The JSON will be like:
 * {
 *      "prompt": "monitor a cat",
 *      "big_image": "base64 encoded JPG image, if include_big_image is enabled, otherwise this field is omitted",
 *      "small_image": "base64 encoded JPG image, if include_small_image is enabled, otherwise this field is omitted",
 *      "boxes": [
 *          {
 *              "x": 100,
 *              "y": 100,
 *              "w": 50,
 *              "h": 60,
 *              "score": 0.8,
 *              "target_cls_id": 1 
 *          }
 *      ]
 * }
 * 
 */


/**
 * struct contains module_base handler, and configurations for UART alarm.
 * 
 * output_format: int, controls what format of the payloda will be put out of the UART.
 * - 0: binary output
 * - 1: JSON output
 * 
 * include_big_image: boolean (true | false), controls whether the big image is included in the output
 * include_small_image: boolean (true | false), controls whether the small image is included in the output
 * include_boxes: boolean (true | false), controls whether the boxes are included in the output
 * 
 * an example for the `params` object of the UART alarm module in the task flow JSON
 * {
 *      "output_format": 1,
 *      "include_big_image": true
 * }
 * 
 * Note: if any configuration fiels is omitted, the default value will imply.
*/
typedef struct {
    tf_module_t module_base;
    int input_evt_id;           //this can also be the module instance id
    int output_format;          //default 0, see comment above
    bool include_big_image;     //default: false
    bool include_small_image;   //default: false
    bool include_boxes;         //default: false, coming soon
} tf_module_uart_alarm_t;


/**
 * instance a uart alarm module
 * 
 * return:
 * - tf_module_t *: a pointer to the tf_module_t base struct, which will be registered into the
 *                  task flow engine.
*/
tf_module_t *tf_module_uart_alarm_instance(void);

/**
 * destroy an instance of uart alarm module
 * 
 * params:
 * - p_module_base: a pointer to the tf_module_t base struct which is obtained from the
 *                  `tf_module_uart_alarm_instance` call.
*/
void tf_module_uart_alarm_destroy(tf_module_t *p_module_base);

/**
 * register the uarl alarm module into the task flow engine
*/
esp_err_t tf_module_uart_alarm_register(void);

#ifdef __cplusplus
}
#endif