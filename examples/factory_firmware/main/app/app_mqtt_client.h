

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "cJSON.h"

#include "data_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_mqtt_client_init(void);

esp_err_t app_mqtt_client_report_tasklist_ack(char *request_id, cJSON *task_settings_node);

esp_err_t app_mqtt_client_report_tasklist_status(intmax_t tasklist_id, int tasklist_status_num);

esp_err_t app_mqtt_client_report_warn_event(intmax_t tasklist_id, char *tasklist_name, int warn_type);

esp_err_t app_mqtt_client_report_device_status(struct view_data_device_status *dev_status);


#ifdef __cplusplus
}
#endif