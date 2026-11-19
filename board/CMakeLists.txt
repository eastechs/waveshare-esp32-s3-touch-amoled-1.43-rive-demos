idf_component_register(
    SRCS "display.c" "changed_frame.c" "sensors.c" "shake.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../vendor/waveshare"
    PRIV_REQUIRES driver esp_lcd espressif__esp_lcd_sh8601 read_lcd_id_bsp i2c_bsp)
