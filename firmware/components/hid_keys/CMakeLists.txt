# Both transports are always listed so the dependency graph does not depend on
# Kconfig; each source file compiles to nothing unless it is selected.
idf_component_register(
    SRCS "hid_usb.c" "hid_ble.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_hid bt nvs_flash
    PRIV_REQUIRES esp_timer
)
