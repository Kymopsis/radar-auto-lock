#pragma once

/*
 * radar_link: reads the 60 GHz radar's UART, decodes TinyFrame packets and
 * keeps the latest target list (type 0x0A04) and point cloud (type 0x0A08).
 *
 * Module frame: +X = left, +Y = forward (out of the antenna face), +Z = up.
 */

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "radar_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool link_ok;                /* a valid frame arrived in the last 2 s */
    int baud;                    /* UART rate in use */
    radar_wire_mode_t mode;      /* 2D / 3D / HLK payload layout */
    uint32_t frames;             /* valid frames since boot */
    uint32_t rx_bytes;           /* raw bytes since boot */
    int64_t target_ms;           /* time of the latest target frame, 0 = never */
    int target_count;            /* targets in the latest target frame */
    int blip_count;              /* in-range targets stored below */
    radar_wire_blip_t blips[RADAR_MAX_BLIPS];
    int64_t cloud_ms;            /* time of the latest cloud frame, 0 = never */
    int cloud_count;
    radar_wire_point_t cloud[RADAR_MAX_CLOUD];
} radar_snapshot_t;

esp_err_t radar_link_init(int uart_num, int tx_gpio, int rx_gpio, int rst_gpio);

/* Copies the latest state. Thread safe. */
void radar_link_snapshot(radar_snapshot_t *out);

#ifdef __cplusplus
}
#endif
