#include "main.h"

#include <esp_event.h>
#include <esp_log.h>
#include <peer.h>

#include "nvs_flash.h"
extern "C" void board_init(void)
{
  bsp_io_expander_init();
  lv_disp_t *lvgl_disp = bsp_lvgl_init();
  assert(lvgl_disp != NULL);
  bsp_rgb_init();
  bsp_codec_init();
  bsp_codec_volume_set(100, NULL);
}

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  
  board_init();
  ui_init();
  oai_wifi_init();
  cmd_init();
  peer_init();
  oai_init_audio_capture();
  oai_init_audio_decoder();
  oai_wifi();
  oai_webrtc();
}