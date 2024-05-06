/*
 * SPDX-FileCopyrightText: 2024 Seeed Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "sensecap-watcher.h"

#include "console_wifi.h"
#include "console_ping.h"
#include "console_ifconfig.h"
#include "iperf_cmd.h"

#include "console_gpio.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init(); // Initialize NVS
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());

    ESP_ERROR_CHECK(console_cmd_wifi_register());

    ESP_ERROR_CHECK(console_cmd_ifconfig_register());

    ESP_ERROR_CHECK(console_cmd_ping_register());

    ESP_ERROR_CHECK(console_cmd_gpio_register());

    ESP_ERROR_CHECK(app_register_iperf_commands());

    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());
}
