
#pragma once
#include "tf_module.h"
#include "tf_module_data_type.h"
#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TF_MODULE_LOCAL_ALARM_NAME     "local alarm"
#define TF_MODULE_LOCAL_ALARM_RVERSION "1.0.0"
#define TF_MODULE_LOCAL_ALARM_DESC     "local alarm module"

#define TF_MODULE_LOCAL_ALARM_DEFAULT_AUDIO_FILE  "/spiffs/alarm-di.wav"

struct tf_module_local_alarm_params
{
    bool rgb;
    bool sound;
    int  duration; //seconds
};

struct tf_module_local_alarm_info
{
    int  duration; //seconds
    struct  tf_data_image img;
    bool is_show_img;
};

typedef struct tf_module_local_alarm
{
    tf_module_t module_serv;
    int input_evt_id; // no output
    struct tf_module_local_alarm_params params;
    esp_timer_handle_t timer_handle;
    struct tf_data_buf audio;
} tf_module_local_alarm_t;

tf_module_t * tf_module_local_alarm_init(tf_module_local_alarm_t *p_module_ins);

esp_err_t tf_module_local_alarm_register(void);

#ifdef __cplusplus
}
#endif