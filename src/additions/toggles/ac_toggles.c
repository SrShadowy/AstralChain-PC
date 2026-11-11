/* [extra] Runtime toggles (extra/toggles/ac_toggles.h): plain globals so the ImGui
 * overlay can override env-derived behaviour without relaunching the game. */
#include "additions/toggles/ac_toggles.h"

int g_extra_vtx_guard = -1;
int g_extra_vtx_debug = -1;
int g_extra_fps_override = 0;
int g_extra_async_kill = 0;
const char *g_extra_present_mode = 0;
float g_extra_lod_bias = -1.0f;
float g_extra_lod_min = -1.0f;
int g_extra_no_shadow = -1;
int g_extra_framegen = -1;
int g_extra_fsr_jitter = -1;
float g_extra_fsr_sharpness = -1.0f;
float g_extra_render_scale = -1.0f;
ExtraFsr3State g_extra_fsr3 = { {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, 0u, 0, 0, 0 };

void extra_async_submit_kill(void) {
    if (g_extra_async_kill) return;
    g_extra_async_kill = 1;
    void nvn_async_drain(void);  /* (nvn/objects.c) */
    nvn_async_drain();
}
