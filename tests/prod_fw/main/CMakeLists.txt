idf_component_register(
    SRCS "main.c" "console_gpio.c"  "console_i2c.c" "console_lcd.c"
    INCLUDE_DIRS "."
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
