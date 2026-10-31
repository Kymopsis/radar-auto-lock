#include "sdkconfig.h"

#if CONFIG_AUTOLOCK_HID_USB

#include <string.h>

#include "class/hid/hid_device.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hid_keys.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

static const char *TAG = "hid_usb";

#define KEY_GAP_MS     30
#define REPORT_WAIT_MS 200

static const uint8_t s_report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(),
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

/* One HID keyboard interface; remote wakeup lets the board wake a sleeping PC. */
static const uint8_t s_config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(s_report_desc), 0x81, 8, 10),
};

static const char *s_strings[] = {
    (const char[]){0x09, 0x04}, /* 0: English (0x0409) */
    "Kymopsis",                 /* 1: manufacturer */
    "Radar Auto-Lock",          /* 2: product (overwritten by device_name) */
    "000001",                   /* 3: serial */
    "Radar Auto-Lock Keyboard", /* 4: HID interface */
};

/* ---- TinyUSB callbacks ---- */

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_report_desc;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    /* Caps/Num Lock LED reports from the host: not used. */
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/* ---- helpers ---- */

static esp_err_t send_report(uint8_t modifiers, uint8_t keycode)
{
    for (int waited = 0; !tud_hid_ready(); waited += 5) {
        if (waited >= REPORT_WAIT_MS || !tud_mounted() || tud_suspended()) {
            return ESP_ERR_INVALID_STATE;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    uint8_t keys[6] = {keycode, 0, 0, 0, 0, 0};
    return tud_hid_keyboard_report(0, modifiers, keycode ? keys : NULL) ? ESP_OK : ESP_FAIL;
}

/* ---- public API ---- */

esp_err_t hid_keys_init(const char *device_name)
{
    if (device_name) {
        s_strings[2] = device_name;
    }
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    cfg.descriptor.string = s_strings;
    cfg.descriptor.string_count = sizeof(s_strings) / sizeof(s_strings[0]);
    cfg.descriptor.full_speed_config = s_config_desc;
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&cfg), TAG, "tinyusb install");
    ESP_LOGI(TAG, "USB keyboard started");
    return ESP_OK;
}

hid_link_state_t hid_keys_state(void)
{
    if (!tud_mounted()) {
        return HID_LINK_DOWN;
    }
    return tud_suspended() ? HID_LINK_SLEEPING : HID_LINK_READY;
}

esp_err_t hid_keys_combo(uint8_t modifiers, uint8_t keycode)
{
    ESP_RETURN_ON_FALSE(hid_keys_state() == HID_LINK_READY, ESP_ERR_INVALID_STATE, TAG,
                        "host not ready");
    esp_err_t err = send_report(modifiers, 0);
    vTaskDelay(pdMS_TO_TICKS(KEY_GAP_MS));
    if (err == ESP_OK) {
        err = send_report(modifiers, keycode);
        vTaskDelay(pdMS_TO_TICKS(KEY_GAP_MS));
    }
    /* Always try to release, even after an error. */
    /* Always release, even after an error, and retry so no modifier stays held. */
    esp_err_t rel = ESP_FAIL;
    for (int i = 0; i < 5 && rel != ESP_OK; ++i) {
        rel = send_report(0, 0);
        if (rel != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (rel != ESP_OK) {
        ESP_LOGW(TAG, "key release failed: %s", esp_err_to_name(rel));
    }
    return err != ESP_OK ? err : rel;
}

esp_err_t hid_keys_wake(void)
{
    if (tud_suspended()) {
        ESP_LOGI(TAG, "host asleep: remote wakeup");
        return tinyusb_remote_wakeup();
    }
    return hid_keys_combo(HID_MOD_LSHIFT, 0);
}

void hid_keys_forget_pairing(void)
{
}

const char *hid_keys_transport(void)
{
    return "USB";
}

#endif /* CONFIG_AUTOLOCK_HID_USB */
