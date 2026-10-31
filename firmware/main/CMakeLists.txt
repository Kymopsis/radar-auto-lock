idf_component_register(
    SRCS "app_main.c" "settings.c" "status_led.c" "console.c"
    INCLUDE_DIRS "."
    REQUIRES radar_link presence hid_keys nvs_flash esp_driver_gpio esp_driver_uart esp_timer esp_app_format
)
