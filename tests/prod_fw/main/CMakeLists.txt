idf_component_register(
    SRCS "main.c" "console_gpio.c"  "console_i2c.c" "console_lcd.c" "console_sscma.c" "console_audio.c" "console_device.c" "console_http.c" "console_wifi.c" "console_rtc.c" "console_bat.c"
    INCLUDE_DIRS "."
    EMBED_FILES "favicon.ico" "upload_script.html"
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
