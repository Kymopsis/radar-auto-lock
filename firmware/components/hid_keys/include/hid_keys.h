#pragma once

/*
 * hid_keys: the board shows up on your computer as a keyboard, over USB
 * (default) or Bluetooth LE (menuconfig -> Radar Auto-Lock -> Keyboard link).
 */

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HID modifier bits (byte 0 of a keyboard report) */
#define HID_MOD_LCTRL  0x01
#define HID_MOD_LSHIFT 0x02
#define HID_MOD_LALT   0x04
#define HID_MOD_LGUI   0x08 /* Windows key / Command / Super */

/* HID usage IDs */
#define HID_KEY_L      0x0F
#define HID_KEY_Q      0x14

typedef enum {
    HID_LINK_DOWN = 0,   /* not plugged in / not paired */
    HID_LINK_READY,      /* host is awake and listening */
    HID_LINK_SLEEPING,   /* USB host suspended the bus (PC asleep) */
} hid_link_state_t;

esp_err_t hid_keys_init(const char *device_name);

hid_link_state_t hid_keys_state(void);

/* Presses modifiers, then the key, then releases everything. */
esp_err_t hid_keys_combo(uint8_t modifiers, uint8_t keycode);

/*
 * Wakes the host: USB remote wakeup if the PC sleeps, otherwise a tap of the
 * left Shift key (turns the screen on, shows the sign-in screen, types nothing).
 */
esp_err_t hid_keys_wake(void);

/* Bluetooth: forget all paired computers. USB: no-op. */
void hid_keys_forget_pairing(void);

/* "USB" or "BLE" */
const char *hid_keys_transport(void);

#ifdef __cplusplus
}
#endif
