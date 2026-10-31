#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "presence.h"

typedef enum {
    HOST_OS_WINDOWS = 0, /* Win + L */
    HOST_OS_MACOS,       /* Ctrl + Cmd + Q */
    HOST_OS_LINUX,       /* Super + L (GNOME, KDE, Cinnamon) */
    HOST_OS_COUNT,
} host_os_t;

typedef struct {
    host_os_t os;
    presence_cfg_t presence;
} settings_t;

/* Loads saved settings (or defaults) into *s. */
esp_err_t settings_load(settings_t *s);
esp_err_t settings_save(const settings_t *s);
void settings_defaults(settings_t *s);

const char *settings_os_name(host_os_t os);
