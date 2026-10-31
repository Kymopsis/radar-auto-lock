#pragma once

/*
 * Presence state machine. Pure C, no ESP-IDF dependency, so it runs in the
 * host test as well as on the board.
 *
 *   PRESENT --(nobody seen for away_ms)--> AWAY     emits PRESENCE_LOCK
 *   AWAY    --(seen for return_ms)------>  PRESENT  emits PRESENCE_WAKE
 *                                                   (only if this absence was locked)
 *
 * While the radar link is down nothing changes: a dead radar never locks
 * your PC.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float range_m;          /* a target counts when it is closer than this */
    float half_fov_deg;     /* ... and within +/- this angle of straight ahead */
    int cloud_min_points;   /* point-cloud points that count as "seen"; 0 = off */
    uint32_t away_ms;       /* nobody seen this long -> lock */
    uint32_t return_ms;     /* seen continuously this long -> wake */
    bool lock_enabled;
    bool wake_enabled;
} presence_cfg_t;

typedef enum {
    PRESENCE_STATE_PRESENT = 0,
    PRESENCE_STATE_AWAY,
} presence_state_t;

typedef enum {
    PRESENCE_EVENT_NONE = 0,
    PRESENCE_EVENT_LOCK,
    PRESENCE_EVENT_WAKE,
} presence_event_t;

typedef struct {
    presence_state_t state;
    bool armed;             /* someone was seen since boot / since the last lock */
    bool seen_once;
    bool locked;            /* a LOCK was sent for the current absence */
    int64_t last_seen_ms;
    int64_t streak_start_ms;
    int64_t streak_last_ms;
} presence_t;

typedef struct {
    float x_m, y_m, z_m;
} presence_point_t;

void presence_default_cfg(presence_cfg_t *cfg);

void presence_init(presence_t *p, int64_t now_ms);

/* True when this point is inside the watched zone. */
bool presence_point_in_zone(const presence_cfg_t *cfg, float x_m, float y_m, float z_m);

/*
 * Call every tick. link_ok = radar frames are arriving; seen = somebody is in
 * the zone in the latest frame.
 */
presence_event_t presence_update(presence_t *p, const presence_cfg_t *cfg, int64_t now_ms,
                                 bool link_ok, bool seen);

/* Milliseconds since somebody was last seen (0 while seen). */
int64_t presence_idle_ms(const presence_t *p, int64_t now_ms);

#ifdef __cplusplus
}
#endif
