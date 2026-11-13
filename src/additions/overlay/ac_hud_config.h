// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Runtime Performance HUD configuration struct shared by ac_hud and ac_config_sync.
#pragma once

#include <stdbool.h>

#define AC_PERF_HUD_DEFAULT_CORNER 1
#define AC_PERF_HUD_DEFAULT_OPACITY 0.86f
#define AC_PERF_HUD_OPACITY_MIN 0.30f
#define AC_PERF_HUD_OPACITY_MAX 1.00f
#define AC_PERF_HUD_MODE_MAX 2
#define AC_PERF_HUD_CORNER_MAX 3

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AcPerfHudConfig {
    bool enabled;
    int mode;
    int corner;
    float opacity;
    bool show_with_menu;
} AcPerfHudConfig;

void ac_perf_hud_get_config(AcPerfHudConfig *out);
void ac_perf_hud_set_config(const AcPerfHudConfig *cfg);

#ifdef __cplusplus
}
#endif
