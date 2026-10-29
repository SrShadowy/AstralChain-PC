/* Astral Chain shim for LASRecomp's settings: fixed defaults, no settings UI.
 * Enum values follow LASRecomp runtime/settings.h. */
#pragma once
#include <stdlib.h> // [extra]
#include <string.h> // [extra]
enum { UPSCALE_BILINEAR = 0, UPSCALE_METALFX = 1, UPSCALE_FSR1 = 2 };
enum { AA_OFF = 0, AA_SMAA = 1 };
typedef struct { int antialias, fullscreen, upscaler; } Settings;
extern Settings g_settings;
static inline void settings_load(void) { // [extra]
    const char *u = getenv("AC_UPSCALER");
    if (u) {
        if (!strcmp(u, "fsr1") || !strcmp(u, "fsr") || !strcmp(u, "2")) g_settings.upscaler = UPSCALE_FSR1;
        else if (!strcmp(u, "bilinear") || !strcmp(u, "0")) g_settings.upscaler = UPSCALE_BILINEAR;
    }
    const char *aa = getenv("AC_ANTIALIAS");
    if (aa) {
        if (!strcmp(aa, "smaa") || !strcmp(aa, "1")) g_settings.antialias = AA_SMAA;
        else if (!strcmp(aa, "off") || !strcmp(aa, "0")) g_settings.antialias = AA_OFF;
    }
} // [extra]
