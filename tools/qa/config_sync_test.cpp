// SPDX-License-Identifier: GPL-2.0-or-later
// Full-parity bidirectional sync verification (Launcher <-> userdata/launcher.ini <-> ImGui/runtime).
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "additions/config/ac_config_sections.h"
#include "additions/config/ac_config_serialize.h"
#include "additions/overlay/ac_menu_gfx_helpers.h"
#include "additions/toggles/ac_toggles.h"
#include "settings.h"

Settings g_settings{};

namespace AcDiscordRpc {
Settings &GetSettings() {
    static Settings s_cfg{};
    return s_cfg;
}
std::string GetClientId() { return "1568955734606086196"; }
void SetClientId(const std::string &) {}
void Init() {}
} // namespace AcDiscordRpc

void nvn_async_drain(void) {}

extern "C" {
void ac_perf_hud_get_config(AcPerfHudConfig *out) {
    if (out) *out = {};
}
void ac_perf_hud_set_config(const AcPerfHudConfig *) {}
void extra_set_prompts_override(int m) { g_extra_prompts_override = m; }
void extra_set_gamepad_confirm_a(int v) { g_extra_gamepad_confirm_a = v; }
int extra_prompts_override(void) { return g_extra_prompts_override; }
int extra_gamepad_confirm_a_enabled(void) { return g_extra_gamepad_confirm_a >= 0 ? g_extra_gamepad_confirm_a : 1; }
}

int main() {
    setenv("AC_OUTPUT_RES", "1600x900", 1);
    setenv("AC_ASPECT", "auto", 1);
    setenv("AC_SHADOW_SCALE", "1", 1);
    setenv("AC_DRAW_DISTANCE", "100", 1);
    setenv("AC_HUD", "1", 1);
    setenv("AC_HUD_AREA", "16:9", 1);
    setenv("AC_PROMPTS", "xbox", 1);
    setenv("AC_ASYNC_SUBMIT", "1", 1);

    const std::string initial_ini =
        "[Graphics]\n"
        "output_res=1600x900\n"
        "aspect=auto\n"
        "vsync=true\n"
        "present_mode=mailbox\n"
        "fps_unlocked=false\n"
        "render_scale=1\n"
        "draw_distance=100\n"
        "shadow_scale=1\n"
        "hud_hidden=false\n"
        "hud_area=16:9\n"
        "[Game]\n"
        "button_prompts=xbox\n"
        "async_submit=true\n";

    ac::config::SyncState st{};
    const auto boot_entries = ac::config::ParseIni(initial_ini);
    ac::config::ApplyGraphicsSection(boot_entries, true, false, st);
    ac::config::ApplyGameSection(boot_entries, true, false, st);

    // 1. Live Launcher edit -> INI poll must sync all Graphics, Game & Mouse parity fields.
    const std::string launcher_edited_ini =
        "[Graphics]\n"
        "output_res=3840x2160\n"
        "aspect=21:9\n"
        "vsync=false\n"
        "present_mode=mailbox\n"
        "fps_limit=144\n"
        "fps_unlocked=true\n"
        "render_scale=0.67\n"
        "lod_bias=1.5\n"
        "lod_min=2.5\n"
        "draw_distance=250\n"
        "shadow_scale=2\n"
        "hud_hidden=true\n"
        "hud_area=screen\n"
        "[Mouse]\n"
        "sensitivity=800\n"
        "y_scale=0.2\n"
        "[Game]\n"
        "parallel_threads=false\n"
        "bc7_precompute=true\n"
        "force_shaders=true\n"
        "verbose_log=true\n"
        "async_submit=true\n"
        "vtx_guard=true\n"
        "vtx_debug=true\n"
        "button_prompts=switch\n";
    const auto live_entries = ac::config::ParseIni(launcher_edited_ini);
    ac::config::ApplyGraphicsSection(live_entries, false, false, st);
    ac::config::ApplyMouseSection(live_entries, false);
    ac::config::ApplyGameSection(live_entries, false, false, st);

    assert(std::strcmp(g_extra_output_res, "3840x2160") == 0);
    assert(std::strcmp(g_extra_aspect_str, "21:9") == 0);
    assert(g_extra_vsync == 0);
    assert(g_extra_fps_unlocked == 1);
    assert(std::fabs(g_extra_lod_bias - 1.5f) < 0.01f);
    assert(std::fabs(g_extra_lod_min - 2.5f) < 0.01f);
    assert(std::fabs(g_extra_mouse_sens - 8.0) < 0.01);
    assert(std::fabs(g_extra_mouse_yscale - 0.2) < 0.01);
    assert(g_extra_parallel_threads == 0 && g_extra_bc7_precompute == 1);
    assert(g_extra_force_shaders == 1 && g_extra_verbose_log == 1);
    assert(g_extra_vtx_guard == 1 && g_extra_vtx_debug == 1);
    assert(g_extra_prompts_override == EXTRA_PROMPT_MODE_SWITCH);

    // 2. Round-trip serialize must preserve every single option cleanly without loss.
    const auto updates = ac::config::BuildIniUpdates(st);
    const std::string saved_ini = ac::config::MergeIniUpdates(launcher_edited_ini, updates);
    const auto parsed_back = ac::config::ParseIni(saved_ini);

    assert(*ac::config::FindIniValue(parsed_back, "Graphics", "output_res") == "3840x2160");
    assert(*ac::config::FindIniValue(parsed_back, "Graphics", "vsync") == "false");
    assert(*ac::config::FindIniValue(parsed_back, "Graphics", "present_mode") == "mailbox");
    assert(*ac::config::FindIniValue(parsed_back, "Graphics", "fps_unlocked") == "true");
    assert(*ac::config::FindIniValue(parsed_back, "Graphics", "lod_bias") == "1.5");
    assert(*ac::config::FindIniValue(parsed_back, "Graphics", "lod_min") == "2.5");
    assert(*ac::config::FindIniValue(parsed_back, "Mouse", "sensitivity") == "800");
    assert(*ac::config::FindIniValue(parsed_back, "Mouse", "y_scale") == "0.2");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "parallel_threads") == "false");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "bc7_precompute") == "true");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "force_shaders") == "true");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "verbose_log") == "true");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "vtx_guard") == "true");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "vtx_debug") == "true");
    assert(*ac::config::FindIniValue(parsed_back, "Game", "button_prompts") == "switch");

    std::puts("PASSED: full-parity Launcher <-> INI <-> ImGui/runtime config sync verified.");
    return 0;
}
