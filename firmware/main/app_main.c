/*
 * Radar Auto-Lock: locks your computer when you walk away from the desk and
 * wakes it when you come back. A 60 GHz radar watches the seat in front of the
 * monitor; the ESP32-S3 acts as a keyboard and presses the lock shortcut.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "console.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hid_keys.h"
#include "nvs_flash.h"
#include "presence.h"
#include "radar_link.h"
#include "status_led.h"

static const char *TAG = "autolock";

#define TICK_MS            50
#define FRAME_FRESH_MS     600
#define LONG_PRESS_MS      3000
#define RESET_PRESS_MS     10000
#define LOG_EVERY_MS       10000
#define WATCH_EVERY_MS     500

typedef enum {
    REQ_NONE = 0,
    REQ_LOCK,
    REQ_WAKE,
} request_t;

static SemaphoreHandle_t s_mutex;
static settings_t s_settings;
static presence_t s_presence;
static radar_snapshot_t s_snap; /* large; static */
static bool s_seen;
static int s_zone_targets;
static int s_zone_points;
static volatile bool s_watch;
static volatile request_t s_request;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* ---------- keyboard actions ---------- */

static esp_err_t send_lock(host_os_t os)
{
    switch (os) {
    case HOST_OS_MACOS:
        return hid_keys_combo(HID_MOD_LCTRL | HID_MOD_LGUI, HID_KEY_Q);
    case HOST_OS_LINUX:
    case HOST_OS_WINDOWS:
    default:
        return hid_keys_combo(HID_MOD_LGUI, HID_KEY_L);
    }
}

static void do_lock(host_os_t os)
{
    esp_err_t err = send_lock(os);
    ESP_LOGI(TAG, "LOCK (%s) -> %s", settings_os_name(os), esp_err_to_name(err));
    if (err == ESP_OK) {
        status_led_flash(LED_WHITE, 2);
    }
}

static void do_wake(void)
{
    esp_err_t err = hid_keys_wake();
    ESP_LOGI(TAG, "WAKE -> %s", esp_err_to_name(err));
    if (err == ESP_OK) {
        status_led_flash(LED_WHITE, 1);
    }
}

/* ---------- radar -> "is somebody at the desk" ---------- */

static bool evaluate_seen(const radar_snapshot_t *snap, const presence_cfg_t *cfg, int64_t t)
{
    s_zone_targets = 0;
    s_zone_points = 0;
    if (snap->target_ms && t - snap->target_ms < FRAME_FRESH_MS) {
        for (int i = 0; i < snap->blip_count; ++i) {
            const radar_wire_blip_t *b = &snap->blips[i];
            if (presence_point_in_zone(cfg, b->x_m, b->y_m, b->z_m)) {
                s_zone_targets++;
            }
        }
    }
    if (cfg->cloud_min_points > 0 && snap->cloud_ms && t - snap->cloud_ms < FRAME_FRESH_MS) {
        for (int i = 0; i < snap->cloud_count; ++i) {
            const radar_wire_point_t *p = &snap->cloud[i];
            if (presence_point_in_zone(cfg, p->x_m, p->y_m, p->z_m)) {
                s_zone_points++;
            }
        }
    }
    return s_zone_targets > 0 ||
           (cfg->cloud_min_points > 0 && s_zone_points >= cfg->cloud_min_points);
}

/* ---------- LED ---------- */

static void update_led(void)
{
    if (hid_keys_state() == HID_LINK_DOWN) {
        status_led_set(LED_PURPLE, LED_BLINK);
    } else if (!s_snap.link_ok) {
        status_led_set(LED_RED, LED_BLINK);
    } else if (!s_settings.presence.lock_enabled) {
        status_led_set(LED_AMBER, LED_SOLID);
    } else if (s_presence.state == PRESENCE_STATE_AWAY) {
        status_led_set(LED_BLUE, LED_SOLID);
    } else {
        status_led_set(LED_GREEN, LED_SOLID);
    }
}

/* ---------- BOOT button ---------- */

static void button_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_AUTOLOCK_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
}

/*
 * Short press: pause / resume auto-lock.
 * Hold 3 s:    next OS (Windows -> macOS -> Linux), shown as 1/2/3 cyan flashes.
 * Hold 10 s:   factory reset (settings and Bluetooth pairings).
 */
static void button_poll(int64_t t)
{
    static int64_t pressed_at;
    static bool cue_long, cue_reset;
    const bool down = gpio_get_level(CONFIG_AUTOLOCK_BUTTON_GPIO) == 0;

    if (down && pressed_at == 0) {
        pressed_at = t;
        cue_long = cue_reset = false;
    } else if (down) {
        const int64_t held = t - pressed_at;
        if (held >= RESET_PRESS_MS && !cue_reset) {
            cue_reset = true;
            status_led_flash(LED_RED, 3);
        } else if (held >= LONG_PRESS_MS && !cue_long) {
            cue_long = true;
            status_led_flash(LED_CYAN, 1);
        }
    } else if (pressed_at != 0) {
        const int64_t held = t - pressed_at;
        pressed_at = 0;
        settings_t s;
        app_get_settings(&s);
        if (held >= RESET_PRESS_MS) {
            ESP_LOGW(TAG, "factory reset");
            hid_keys_forget_pairing();
            settings_defaults(&s);
            app_apply_settings(&s);
            status_led_flash(LED_RED, 3);
        } else if (held >= LONG_PRESS_MS) {
            s.os = (host_os_t)((s.os + 1) % HOST_OS_COUNT);
            app_apply_settings(&s);
            ESP_LOGI(TAG, "OS -> %s", settings_os_name(s.os));
            status_led_flash(LED_CYAN, (int)s.os + 1);
        } else if (held >= 50) {
            s.presence.lock_enabled = !s.presence.lock_enabled;
            app_apply_settings(&s);
            ESP_LOGI(TAG, "auto-lock %s", s.presence.lock_enabled ? "ON" : "PAUSED");
        }
    }
}

/* ---------- console accessors ---------- */

void app_get_settings(settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_mutex);
}

void app_apply_settings(const settings_t *s)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings = *s;
    xSemaphoreGive(s_mutex);
    esp_err_t err = settings_save(s);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
}

void app_set_watch(bool on)
{
    s_watch = on;
}

void app_request_lock(void)
{
    s_request = REQ_LOCK;
}

void app_request_wake(void)
{
    s_request = REQ_WAKE;
}

static const char *mode_name(radar_wire_mode_t m)
{
    switch (m) {
    case RADAR_WIRE_2D: return "2D";
    case RADAR_WIRE_3D: return "3D";
    case RADAR_WIRE_HLK: return "HLK";
    default: return "?";
    }
}

static const char *link_name(hid_link_state_t s)
{
    switch (s) {
    case HID_LINK_READY: return "ready";
    case HID_LINK_SLEEPING: return "host asleep";
    default: return "not connected";
    }
}

void app_print_status(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int64_t t = now_ms();
    printf("keyboard  %s, %s\n", hid_keys_transport(), link_name(hid_keys_state()));
    printf("radar     %s, %d baud, %s, frames %" PRIu32 ", bytes %" PRIu32 "\n",
           s_snap.link_ok ? "OK" : "NO DATA", s_snap.baud, mode_name(s_snap.mode), s_snap.frames,
           s_snap.rx_bytes);
    printf("zone      targets %d (of %d), cloud points %d\n", s_zone_targets, s_snap.target_count,
           s_zone_points);
    for (int i = 0; i < s_snap.blip_count; ++i) {
        const radar_wire_blip_t *b = &s_snap.blips[i];
        printf("  target  %.2f m at %+.0f deg (x %.2f y %.2f z %.2f)\n", b->distance_m,
               b->azimuth_deg, b->x_m, b->y_m, b->z_m);
    }
    printf("state     %s, %s, idle %.1f s of %.1f s\n",
           s_presence.state == PRESENCE_STATE_AWAY ? "AWAY" : "PRESENT",
           s_settings.presence.lock_enabled ? "auto-lock on" : "PAUSED",
           presence_idle_ms(&s_presence, t) / 1000.0, s_settings.presence.away_ms / 1000.0);
    xSemaphoreGive(s_mutex);
}

/* ---------- main loop ---------- */

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    s_mutex = xSemaphoreCreateMutex();
    settings_load(&s_settings);
    ESP_LOGI(TAG, "Radar Auto-Lock %s, OS %s, lock after %" PRIu32 " s",
             esp_app_get_description()->version, settings_os_name(s_settings.os),
             s_settings.presence.away_ms / 1000);

    status_led_init(CONFIG_AUTOLOCK_LED_GPIO);
    button_init();
    ESP_ERROR_CHECK(hid_keys_init("Radar Auto-Lock"));
    ESP_ERROR_CHECK(radar_link_init(1, CONFIG_AUTOLOCK_RADAR_TX_GPIO, CONFIG_AUTOLOCK_RADAR_RX_GPIO,
                                    CONFIG_AUTOLOCK_RADAR_RST_GPIO));
    console_start();

    presence_init(&s_presence, now_ms());
    int64_t last_log = 0;
    bool was_link_ok = false;

    for (;;) {
        const int64_t t = now_ms();

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        radar_link_snapshot(&s_snap);
        const settings_t cfg = s_settings;
        s_seen = evaluate_seen(&s_snap, &cfg.presence, t);
        const presence_event_t ev =
            presence_update(&s_presence, &cfg.presence, t, s_snap.link_ok, s_seen);
        xSemaphoreGive(s_mutex);

        if (s_snap.link_ok != was_link_ok) {
            was_link_ok = s_snap.link_ok;
            ESP_LOGI(TAG, "radar %s (%d baud)", was_link_ok ? "connected" : "lost", s_snap.baud);
        }

        if (ev == PRESENCE_EVENT_LOCK) {
            do_lock(cfg.os);
        } else if (ev == PRESENCE_EVENT_WAKE) {
            do_wake();
        }

        const request_t req = s_request;
        s_request = REQ_NONE;
        if (req == REQ_LOCK) {
            do_lock(cfg.os);
        } else if (req == REQ_WAKE) {
            do_wake();
        }

        button_poll(t);
        update_led();

        if (s_watch && t - last_log >= WATCH_EVERY_MS) {
            last_log = t;
            printf("[watch] %s seen=%d targets=%d points=%d idle=%.1fs\n",
                   s_presence.state == PRESENCE_STATE_AWAY ? "AWAY" : "PRESENT", s_seen,
                   s_zone_targets, s_zone_points, presence_idle_ms(&s_presence, t) / 1000.0);
        } else if (!s_watch && t - last_log >= LOG_EVERY_MS) {
            last_log = t;
            ESP_LOGI(TAG, "%s, radar %s, keyboard %s",
                     s_presence.state == PRESENCE_STATE_AWAY ? "away" : "present",
                     s_snap.link_ok ? "ok" : "no data", link_name(hid_keys_state()));
        }

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
