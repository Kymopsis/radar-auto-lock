#include "settings.h"

#include <string.h>

#include "nvs.h"

#define NS "autolock"

void settings_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->os = HOST_OS_WINDOWS;
    presence_default_cfg(&s->presence);
}

static int32_t get_i32(nvs_handle_t h, const char *key, int32_t fallback)
{
    int32_t v;
    return nvs_get_i32(h, key, &v) == ESP_OK ? v : fallback;
}

esp_err_t settings_load(settings_t *s)
{
    settings_defaults(s);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }
    presence_cfg_t *p = &s->presence;
    int32_t os = get_i32(h, "os", s->os);
    s->os = (os >= 0 && os < HOST_OS_COUNT) ? (host_os_t)os : HOST_OS_WINDOWS;
    p->range_m = (float)get_i32(h, "range_cm", (int32_t)(p->range_m * 100.0f)) / 100.0f;
    p->half_fov_deg = (float)get_i32(h, "fov_deg", (int32_t)p->half_fov_deg);
    p->cloud_min_points = get_i32(h, "cloud_min", p->cloud_min_points);
    p->use_radar_zones = get_i32(h, "zones", p->use_radar_zones) != 0;
    p->away_ms = (uint32_t)get_i32(h, "away_ms", (int32_t)p->away_ms);
    p->return_ms = (uint32_t)get_i32(h, "return_ms", (int32_t)p->return_ms);
    p->lock_enabled = get_i32(h, "lock", p->lock_enabled) != 0;
    p->wake_enabled = get_i32(h, "wake", p->wake_enabled) != 0;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t settings_save(const settings_t *s)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    const presence_cfg_t *p = &s->presence;
    nvs_set_i32(h, "os", s->os);
    nvs_set_i32(h, "range_cm", (int32_t)(p->range_m * 100.0f + 0.5f));
    nvs_set_i32(h, "fov_deg", (int32_t)(p->half_fov_deg + 0.5f));
    nvs_set_i32(h, "cloud_min", p->cloud_min_points);
    nvs_set_i32(h, "zones", p->use_radar_zones);
    nvs_set_i32(h, "away_ms", (int32_t)p->away_ms);
    nvs_set_i32(h, "return_ms", (int32_t)p->return_ms);
    nvs_set_i32(h, "lock", p->lock_enabled);
    nvs_set_i32(h, "wake", p->wake_enabled);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

const char *settings_os_name(host_os_t os)
{
    switch (os) {
    case HOST_OS_MACOS:
        return "macos";
    case HOST_OS_LINUX:
        return "linux";
    default:
        return "windows";
    }
}
