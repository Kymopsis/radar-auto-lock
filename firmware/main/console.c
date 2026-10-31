#include "console.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hid_keys.h"
#include "sdkconfig.h"

static const char *TAG = "console";

#define CONSOLE_LINE_MAX 96

static void print_help(void)
{
    printf("\nRadar Auto-Lock commands:\n"
           "  status                 radar, keyboard and presence state\n"
           "  watch on|off           print presence twice a second\n"
           "  settings               show settings\n"
           "  set os windows|macos|linux\n"
           "  set away <seconds>     lock after nobody is seen this long (5-600)\n"
           "  set range <metres>     watched distance (0.3-3.0)\n"
           "  set fov <degrees>      watched half-angle left/right (10-60)\n"
           "  set return <ms>        seen this long counts as back (200-5000)\n"
           "  set cloud <points>     point-cloud points that count as seen, 0 = off\n"
           "  set lock on|off        auto-lock (BOOT button toggles it too)\n"
           "  set wake on|off        wake the screen when you come back\n"
           "  lock / wake            send it now (test the keyboard)\n"
           "  unpair                 forget Bluetooth pairings\n"
           "  defaults               restore defaults\n\n");
}

static void print_settings(void)
{
    settings_t s;
    app_get_settings(&s);
    const presence_cfg_t *p = &s.presence;
    printf("os %s | away %.0f s | range %.2f m | fov %.0f deg | return %u ms | cloud %d | lock %s | wake %s\n",
           settings_os_name(s.os), p->away_ms / 1000.0, p->range_m, p->half_fov_deg,
           (unsigned)p->return_ms, p->cloud_min_points, p->lock_enabled ? "on" : "off",
           p->wake_enabled ? "on" : "off");
}

static bool parse_onoff(const char *v, bool *out)
{
    if (!strcmp(v, "on") || !strcmp(v, "1")) {
        *out = true;
        return true;
    }
    if (!strcmp(v, "off") || !strcmp(v, "0")) {
        *out = false;
        return true;
    }
    return false;
}

static bool parse_num(const char *v, float lo, float hi, float *out)
{
    char *end;
    const float f = strtof(v, &end);
    if (end == v || *end != '\0' || f < lo || f > hi) {
        return false;
    }
    *out = f;
    return true;
}

static void cmd_set(const char *key, const char *val)
{
    settings_t s;
    app_get_settings(&s);
    presence_cfg_t *p = &s.presence;
    float f;
    bool b;
    bool ok = true;

    if (!strcmp(key, "os")) {
        if (!strcmp(val, "windows")) {
            s.os = HOST_OS_WINDOWS;
        } else if (!strcmp(val, "macos") || !strcmp(val, "mac")) {
            s.os = HOST_OS_MACOS;
        } else if (!strcmp(val, "linux")) {
            s.os = HOST_OS_LINUX;
        } else {
            ok = false;
        }
    } else if (!strcmp(key, "away") && (ok = parse_num(val, 5, 600, &f))) {
        p->away_ms = (uint32_t)(f * 1000.0f);
    } else if (!strcmp(key, "range") && (ok = parse_num(val, 0.3f, 3.0f, &f))) {
        p->range_m = f;
    } else if (!strcmp(key, "fov") && (ok = parse_num(val, 10, 60, &f))) {
        p->half_fov_deg = f;
    } else if (!strcmp(key, "return") && (ok = parse_num(val, 200, 5000, &f))) {
        p->return_ms = (uint32_t)f;
    } else if (!strcmp(key, "cloud") && (ok = parse_num(val, 0, 64, &f))) {
        p->cloud_min_points = (int)f;
    } else if (!strcmp(key, "lock") && (ok = parse_onoff(val, &b))) {
        p->lock_enabled = b;
    } else if (!strcmp(key, "wake") && (ok = parse_onoff(val, &b))) {
        p->wake_enabled = b;
    } else if (ok) {
        printf("unknown setting '%s' (type help)\n", key);
        return;
    }
    if (!ok) {
        printf("bad value '%s' for %s (type help)\n", val, key);
        return;
    }
    app_apply_settings(&s);
    print_settings();
}

static void handle_line(char *line)
{
    char *argv[3] = {0};
    int argc = 0;
    for (char *tok = strtok(line, " \t"); tok && argc < 3; tok = strtok(NULL, " \t")) {
        for (char *c = tok; *c; ++c) {
            *c = (char)tolower((unsigned char)*c);
        }
        argv[argc++] = tok;
    }
    if (argc == 0) {
        return;
    }
    const char *cmd = argv[0];
    if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
        print_help();
    } else if (!strcmp(cmd, "status")) {
        app_print_status();
    } else if (!strcmp(cmd, "settings") || !strcmp(cmd, "get")) {
        print_settings();
    } else if (!strcmp(cmd, "watch")) {
        bool on = true;
        if (argc > 1 && !parse_onoff(argv[1], &on)) {
            printf("watch on|off\n");
            return;
        }
        app_set_watch(on);
    } else if (!strcmp(cmd, "set") && argc == 3) {
        cmd_set(argv[1], argv[2]);
    } else if (!strcmp(cmd, "lock")) {
        app_request_lock();
    } else if (!strcmp(cmd, "wake")) {
        app_request_wake();
    } else if (!strcmp(cmd, "unpair")) {
        hid_keys_forget_pairing();
    } else if (!strcmp(cmd, "defaults")) {
        settings_t s;
        settings_defaults(&s);
        app_apply_settings(&s);
        print_settings();
    } else {
        printf("unknown command (type help)\n");
    }
}

static void console_task(void *arg)
{
    (void)arg;
    const uart_port_t port = (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM;
    char line[CONSOLE_LINE_MAX];
    size_t len = 0;
    uint8_t c;
    print_help();
    for (;;) {
        if (uart_read_bytes(port, &c, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (len > 0) {
                printf("\n");
                line[len] = '\0';
                handle_line(line);
                len = 0;
            }
        } else if (c == 0x08 || c == 0x7F) {
            if (len > 0) {
                len--;
                printf("\b \b");
            }
        } else if (isprint(c) && len < CONSOLE_LINE_MAX - 1) {
            line[len++] = (char)c;
            putchar(c); /* echo */
        }
        fflush(stdout);
    }
}

void console_start(void)
{
#if CONFIG_ESP_CONSOLE_UART
    const uart_port_t port = (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM;
    if (!uart_is_driver_installed(port)) {
        if (uart_driver_install(port, 256, 0, 0, NULL, 0) != ESP_OK) {
            ESP_LOGE(TAG, "UART%d driver install failed", port);
            return;
        }
    }
    xTaskCreate(console_task, "console", 4096, NULL, 3, NULL);
#else
    ESP_LOGW(TAG, "console needs CONFIG_ESP_CONSOLE_UART");
#endif
}
