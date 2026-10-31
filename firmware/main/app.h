#pragma once

#include <stdbool.h>

#include "settings.h"

/* Thread-safe accessors used by the serial console. */
void app_get_settings(settings_t *out);
void app_apply_settings(const settings_t *s); /* applies and saves */
void app_print_status(void);
void app_set_watch(bool on);
void app_request_lock(void);
void app_request_wake(void);
