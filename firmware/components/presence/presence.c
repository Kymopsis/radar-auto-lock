#include "presence.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Gaps shorter than this keep a "returning" streak alive (frames drop out). */
#define STREAK_GAP_MS 400

void presence_default_cfg(presence_cfg_t *cfg)
{
    cfg->range_m = 1.2f;
    cfg->half_fov_deg = 60.0f;
    cfg->cloud_min_points = 3;
    cfg->use_radar_zones = false;
    cfg->away_ms = 30000;
    cfg->return_ms = 1000;
    cfg->lock_enabled = true;
    cfg->wake_enabled = true;
}

void presence_init(presence_t *p, int64_t now_ms)
{
    memset(p, 0, sizeof(*p));
    p->state = PRESENCE_STATE_PRESENT;
    p->last_seen_ms = now_ms;
}

bool presence_point_in_zone(const presence_cfg_t *cfg, float x_m, float y_m, float z_m)
{
    const float d = sqrtf(x_m * x_m + y_m * y_m + z_m * z_m);
    if (!isfinite(d) || d > cfg->range_m || y_m <= 0.0f) {
        return false;
    }
    const float az = atan2f(x_m, y_m) * (180.0f / (float)M_PI);
    return fabsf(az) <= cfg->half_fov_deg;
}

presence_event_t presence_update(presence_t *p, const presence_cfg_t *cfg, int64_t now_ms,
                                 bool link_ok, bool seen)
{
    if (!link_ok) {
        /* Freeze timers so a radar dropout is not mistaken for "away". */
        p->last_seen_ms = now_ms;
        p->streak_start_ms = 0;
        return PRESENCE_EVENT_NONE;
    }

    if (seen) {
        if (p->streak_start_ms == 0 || now_ms - p->streak_last_ms > STREAK_GAP_MS) {
            p->streak_start_ms = now_ms;
        }
        p->streak_last_ms = now_ms;
        p->last_seen_ms = now_ms;
        p->seen_once = true;
    }

    switch (p->state) {
    case PRESENCE_STATE_PRESENT:
        if (seen) {
            p->armed = true;
        }
        if (now_ms - p->last_seen_ms >= (int64_t)cfg->away_ms) {
            p->state = PRESENCE_STATE_AWAY;
            p->streak_start_ms = 0;
            const bool fire = p->armed && cfg->lock_enabled;
            p->armed = false;
            p->locked = fire;
            return fire ? PRESENCE_EVENT_LOCK : PRESENCE_EVENT_NONE;
        }
        break;
    case PRESENCE_STATE_AWAY:
        if (seen && p->streak_start_ms != 0 &&
            now_ms - p->streak_start_ms >= (int64_t)cfg->return_ms) {
            p->state = PRESENCE_STATE_PRESENT;
            p->armed = true;
            const bool fire = p->locked && cfg->wake_enabled;
            p->locked = false;
            return fire ? PRESENCE_EVENT_WAKE : PRESENCE_EVENT_NONE;
        }
        break;
    }
    return PRESENCE_EVENT_NONE;
}

int64_t presence_idle_ms(const presence_t *p, int64_t now_ms)
{
    const int64_t d = now_ms - p->last_seen_ms;
    return d > 0 ? d : 0;
}
