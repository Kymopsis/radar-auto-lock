#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef enum {
    LED_OFF = 0,
    LED_GREEN,   /* you are at the desk, watching */
    LED_BLUE,    /* you are away (PC locked) */
    LED_AMBER,   /* paused: auto-lock off */
    LED_RED,     /* no radar data */
    LED_PURPLE,  /* keyboard link down (USB not enumerated / Bluetooth not paired) */
    LED_CYAN,    /* settings feedback */
    LED_WHITE,   /* action sent */
} led_color_t;

typedef enum {
    LED_SOLID = 0,
    LED_BLINK,   /* 1 Hz */
} led_mode_t;

esp_err_t status_led_init(int gpio);

/* Background pattern. */
void status_led_set(led_color_t color, led_mode_t mode);

/* Flashes count times on top of the background pattern. */
void status_led_flash(led_color_t color, int count);
