/* [extra] Runtime toggles (additions/toggles/ac_toggles.h): plain globals so the ImGui
 * overlay and live INI config sync can override env-derived behaviour without relaunching. */
#include "additions/toggles/ac_toggles.h"

int g_extra_vtx_guard = EXTRA_TOGGLE_UNSET;
int g_extra_vtx_debug = EXTRA_TOGGLE_UNSET;
int g_extra_fps_override = EXTRA_TOGGLE_UNSET;
int g_extra_async_kill = 0;
const char *g_extra_present_mode = 0;
float g_extra_lod_bias = -1.0f;
float g_extra_lod_min = -1.0f;
int g_extra_no_shadow = EXTRA_TOGGLE_UNSET;
int g_extra_framegen = EXTRA_TOGGLE_UNSET;
int g_extra_fsr_jitter = EXTRA_TOGGLE_UNSET;
float g_extra_fsr_sharpness = -1.0f;
float g_extra_render_scale = -1.0f;

int g_extra_fullscreen_pending = EXTRA_TOGGLE_UNSET;
int g_extra_win_w = 0;
int g_extra_win_h = 0;
int g_extra_win_resize_pending = 0;
int g_extra_config_dirty = 0;
int g_extra_present_pending = 0;
int g_sdl_fullscreen = 0;
int g_sdl_input_focus = 0;
uint32_t g_sdl_prompt_generation = 1;
int g_extra_prompts_override = EXTRA_TOGGLE_UNSET;
int g_extra_gamepad_confirm_a = EXTRA_TOGGLE_UNSET;
int g_extra_ignore_focus = EXTRA_TOGGLE_UNSET;
int g_extra_cutscene_30fps = EXTRA_TOGGLE_UNSET;
int g_extra_audio_volume = EXTRA_TOGGLE_UNSET;
int g_extra_audio_mute_unfocus = EXTRA_TOGGLE_UNSET;
uint32_t g_extra_audio_gen = 1;
int g_extra_mouse_override = 0;
uint32_t g_extra_mouse_gen = 0;
int g_extra_mouse_look = 1;
double g_extra_mouse_sens = 1.0;
double g_extra_mouse_yscale = 1.0;
double g_extra_mouse_curve_exp = EXTRA_MOUSE_CURVE_DEFAULT;
int g_extra_mouse_invert = 0;
int g_extra_mouse_menu_nav = 1;
int g_extra_mouse_menu_motion = 1;
float g_extra_mouse_menu_cell = EXTRA_MOUSE_CELL_DEFAULT;
uint8_t g_extra_key_map[EXTRA_KEY_SLOTS][EXTRA_KEY_ACTIONS] = {{0}};
int g_extra_key_override = 0;
uint32_t g_extra_keys_gen = 0;

ExtraFsr3State g_extra_fsr3 = { {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, 0u, 0, 0, 0 };

void extra_async_submit_kill(void) {
    if (g_extra_async_kill) return;
    g_extra_async_kill = 1;
    void nvn_async_drain(void);  /* (nvn/objects.c) */
    nvn_async_drain();
}

uint32_t extra_sync_read_mouse(int *look, double *sens, double *yscale, double *curve_exp, int *invert) {
    if (!g_extra_mouse_override) return 0;
    if (look) *look = g_extra_mouse_look;
    if (sens) *sens = g_extra_mouse_sens;
    if (yscale) *yscale = g_extra_mouse_yscale;
    if (curve_exp) *curve_exp = g_extra_mouse_curve_exp;
    if (invert) *invert = g_extra_mouse_invert;
    return g_extra_mouse_gen;
}

uint32_t extra_sync_read_menu_mouse(int *nav, int *motion, float *cell) {
    if (!g_extra_mouse_override) return 0;
    if (nav) *nav = g_extra_mouse_menu_nav;
    if (motion) *motion = g_extra_mouse_menu_motion;
    if (cell) *cell = g_extra_mouse_menu_cell;
    return g_extra_mouse_gen;
}

__attribute__((weak)) void ac_config_sync_init_hook(void) {
}

__attribute__((weak)) void ac_config_sync_save_hook(void) {
}
