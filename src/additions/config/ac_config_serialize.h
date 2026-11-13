// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Section-level Game, Overlay, DiscordRPC, Controls appliers and INI serialization.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "additions/config/ac_config_sections.h"
#include "additions/overlay/ac_hud_config.h"
#include "additions/rpc/ac_discord_rpc.h"

namespace ac::config {

inline int ParsePromptModeStr(const std::string& s) {
    if (s == "switch" || s == "0") {
        return EXTRA_PROMPT_MODE_SWITCH;
    }
    if (s == "keyboard" || s == "1") {
        return EXTRA_PROMPT_MODE_KBM;
    }
    if (s == "xbox" || s == "2") {
        return EXTRA_PROMPT_MODE_XBOX;
    }
    return EXTRA_PROMPT_MODE_AUTO;
}

inline void ApplyGameSection(const std::vector<IniEntry>& entries,
                             bool boot,
                             bool legacy_fallback,
                             SyncState& st) {
    if (boot) {
        const char* as = EnvStr("AC_ASYNC_SUBMIT");
        st.boot_async_env = (as && std::atoi(as) > 0);
    } else if (const std::string* vas = FindKey(entries, "Game", "async_submit", false)) {
        if (!ParseBoolVal(*vas, true) && st.boot_async_env && !g_extra_async_kill) {
            extra_async_submit_kill();
        }
    }
    if (const std::string* vif = FindKey(entries, "Game", "ignore_focus", false)) {
        if (!boot || !EnvNonEmpty("AC_IGNORE_FOCUS")) {
            g_extra_ignore_focus = ParseBoolVal(*vif, false) ? 1 : 0;
        }
    }
    if (const std::string* vcs = FindKey(entries, "Game", "cutscene_30fps", false)) {
        if (!boot || !EnvNonEmpty("AC_CUTSCENE_FPS")) {
            g_extra_cutscene_30fps = ParseBoolVal(*vcs, false) ? 1 : 0;
        }
    }
    if (const std::string* vxa = FindKey(entries, "Game", "xbox_confirm_a", false)) {
        if (!boot || !EnvNonEmpty("AC_XBOX_CONFIRM_A")) {
            int want = ParseBoolVal(*vxa, true) ? 1 : 0;
            if (g_extra_gamepad_confirm_a != want) {
                g_extra_gamepad_confirm_a = want;
                if (!boot) {
                    g_sdl_prompt_generation++;
                }
            }
        }
    }
    if (const std::string* vbp = FindKey(entries, "Game", "button_prompts", false)) {
        if (!boot || !EnvNonEmpty("AC_PROMPTS")) {
            int mode = ParsePromptModeStr(*vbp);
            if (g_extra_prompts_override != mode) {
                g_extra_prompts_override = mode;
                if (!boot) {
                    g_sdl_prompt_generation++;
                }
            }
        }
    }
    if (const std::string* vl = FindKey(entries, "Game", "language", legacy_fallback)) {
        if (!boot || (!EnvNonEmpty("AC_LANGUAGE") && !EnvNonEmpty("LARECOMP_LANG"))) {
            for (int i = 0; i < N_LANGUAGES; ++i) {
                if (*vl == g_language_codes[i]) {
                    g_settings.language = i;
                    break;
                }
            }
        }
    }
    if (const std::string* vv = FindKey(entries, "Game", "voice_language", legacy_fallback)) {
        if (!boot || (!EnvNonEmpty("AC_VOICE_LANGUAGE") && !EnvNonEmpty("LARECOMP_VOICE_LANG"))) {
            g_settings.voice_language = (*vv == "ja") ? 1 : (*vv == "en" ? 2 : 0);
        }
    }
}

inline void ApplyOverlayAndRpcSections(const std::vector<IniEntry>& entries, bool boot) {
    AcPerfHudConfig hud{};
    ac_perf_hud_get_config(&hud);
    if (boot) {
        if (const char* eh = EnvStr("AC_PERF_HUD")) {
            int m = std::atoi(eh);
            if (m > 0) {
                hud.enabled = true;
                if (m == kPerfHudLegacyCompact) {
                    hud.mode = 0;
                } else if (m == kPerfHudLegacyDetailed) {
                    hud.mode = 1;
                } else if (m == kPerfHudLegacyMinimal) {
                    hud.mode = 2;
                }
            } else {
                hud.enabled = false;
            }
        }
        if (const char* ec = EnvStr("AC_PERF_HUD_CORNER")) {
            hud.corner = std::clamp(std::atoi(ec), 0, AC_PERF_HUD_CORNER_MAX);
        }
        if (const char* eo = EnvStr("AC_PERF_HUD_OPACITY")) {
            hud.opacity = std::clamp(static_cast<float>(std::atof(eo)),
                                     AC_PERF_HUD_OPACITY_MIN, AC_PERF_HUD_OPACITY_MAX);
        }
        if (const char* ew = EnvStr("AC_PERF_HUD_WITH_MENU")) {
            hud.show_with_menu = ParseBoolVal(ew, hud.show_with_menu);
        }
    }
    if (!boot || !EnvNonEmpty("AC_PERF_HUD")) {
        if (const std::string* v = FindKey(entries, "Overlay", "perf_hud_enabled", false)) {
            hud.enabled = ParseBoolVal(*v, hud.enabled);
        }
        if (const std::string* v = FindKey(entries, "Overlay", "perf_hud_mode", false)) {
            hud.mode = std::clamp(ParseIntVal(*v, hud.mode), 0, AC_PERF_HUD_MODE_MAX);
        }
    }
    if (!boot || !EnvNonEmpty("AC_PERF_HUD_CORNER")) {
        if (const std::string* v = FindKey(entries, "Overlay", "perf_hud_corner", false)) {
            hud.corner = std::clamp(ParseIntVal(*v, hud.corner), 0, AC_PERF_HUD_CORNER_MAX);
        }
    }
    if (!boot || !EnvNonEmpty("AC_PERF_HUD_OPACITY")) {
        if (const std::string* v = FindKey(entries, "Overlay", "perf_hud_opacity", false)) {
            hud.opacity = std::clamp(static_cast<float>(ParseDoubleVal(*v, hud.opacity)),
                                     AC_PERF_HUD_OPACITY_MIN, AC_PERF_HUD_OPACITY_MAX);
        }
    }
    if (!boot || !EnvNonEmpty("AC_PERF_HUD_WITH_MENU")) {
        if (const std::string* v = FindKey(entries, "Overlay", "perf_hud_show_with_menu", false)) {
            hud.show_with_menu = ParseBoolVal(*v, hud.show_with_menu);
        }
    }
    ac_perf_hud_set_config(&hud);

    auto& rpc = AcDiscordRpc::GetSettings();
    if (const std::string* v = FindKey(entries, "DiscordRPC", "enabled", false)) {
        if (!boot || !EnvNonEmpty("AC_DISCORD_RPC")) {
            rpc.enabled.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
        }
    } else if (boot) {
        if (const char* e = EnvStr("AC_DISCORD_RPC")) {
            rpc.enabled.store(std::atoi(e) != 0 && std::strcmp(e, "false") != 0, std::memory_order_relaxed);
        }
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "show_file", false)) {
        rpc.show_file.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "show_mission", false)) {
        rpc.show_mission.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "show_area", false)) {
        rpc.show_area.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "show_boss", false)) {
        rpc.show_boss.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "show_difficulty", false)) {
        rpc.show_difficulty.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "show_time", false)) {
        rpc.show_time.store(ParseBoolVal(*v, true), std::memory_order_relaxed);
    }
    if (const std::string* v = FindKey(entries, "DiscordRPC", "client_id", false)) {
        if (!boot || !EnvNonEmpty("AC_DISCORD_CLIENT_ID")) {
            AcDiscordRpc::SetClientId(v->empty() ? kDefaultRpcId : *v);
        }
    } else if (boot) {
        if (const char* eid = EnvStr("AC_DISCORD_CLIENT_ID")) {
            AcDiscordRpc::SetClientId(eid);
        }
    }
    if (rpc.enabled.load(std::memory_order_relaxed)) {
        AcDiscordRpc::Init();
    }
}

inline void ApplyControlsSection(const std::vector<IniEntry>& entries) {
    uint8_t next_map[EXTRA_KEY_SLOTS][EXTRA_KEY_ACTIONS]{};
    BuildEffectiveKeyMap(entries, next_map);
    if (!g_extra_key_override || std::memcmp(next_map, g_extra_key_map, sizeof(next_map)) != 0) {
        std::memcpy(g_extra_key_map, next_map, sizeof(next_map));
        g_extra_key_override = 1;
        g_extra_keys_gen++;
        g_sdl_prompt_generation++;
    }
}

inline std::vector<IniEntry> BuildIniUpdates(const SyncState& st) {
    std::vector<IniEntry> out;
    auto push = [&out](const char* sec, const char* key, const std::string& val) {
        out.push_back({sec, key, val});
    };

    const int eff_w = g_extra_win_w > 0 ? g_extra_win_w : st.win_w;
    const int eff_h = g_extra_win_h > 0 ? g_extra_win_h : st.win_h;
    const bool eff_fs = (g_extra_fullscreen_pending >= 0 ? g_extra_fullscreen_pending : g_sdl_fullscreen) != 0;
    const char* eff_pm = CanonicalPresentMode(g_extra_present_mode ? g_extra_present_mode : EnvStr("LARECOMP_VK_PRESENT"));
    int eff_fps = g_extra_fps_override >= 0 ? g_extra_fps_override
                : (EnvNonEmpty("AC_FPS") ? std::atoi(EnvStr("AC_FPS"))
                : (EnvNonEmpty("AC_TARGET_FPS") ? std::atoi(EnvStr("AC_TARGET_FPS")) : kDefaultFpsLimit));
    const char* ustr = g_settings.upscaler == UPSCALE_FSR3 ? "fsr3"
                     : (g_settings.upscaler == UPSCALE_FSR1 ? "fsr1" : "bilinear");
    const float eff_rs = g_extra_render_scale > 0.0f ? g_extra_render_scale : g_settings.render_scale;
    const bool eff_fg = (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) != 0;
    const bool eff_jit = (g_extra_fsr_jitter >= 0 ? g_extra_fsr_jitter : g_settings.fsr_jitter) != 0;
    const float eff_shp = g_extra_fsr_sharpness >= 0.0f ? g_extra_fsr_sharpness : g_settings.fsr_sharpness;
    const float eff_lb = g_extra_lod_bias >= 0.0f ? g_extra_lod_bias : (EnvNonEmpty("LARECOMP_LOD_BIAS") ? static_cast<float>(std::atof(EnvStr("LARECOMP_LOD_BIAS"))) : 0.0f);
    const float eff_lm = g_extra_lod_min >= 0.0f ? g_extra_lod_min : (EnvNonEmpty("LARECOMP_LOD_MIN") ? static_cast<float>(std::atof(EnvStr("LARECOMP_LOD_MIN"))) : 0.0f);
    const bool eff_ns = (g_extra_no_shadow >= 0 ? g_extra_no_shadow != 0 : EnvNonEmpty("LARECOMP_NO_SHADOW"));

    push("Graphics", "width", std::to_string(eff_w));
    push("Graphics", "height", std::to_string(eff_h));
    push("Graphics", "fullscreen", FormatBoolVal(eff_fs));
    push("Graphics", "vsync", FormatBoolVal(std::strcmp(eff_pm, kPmImmediate) != 0));
    push("Graphics", "present_mode", eff_pm);
    push("Graphics", "fps_limit", std::to_string(eff_fps));
    push("Graphics", "render_scale", FormatCompactDouble(eff_rs));
    push("Graphics", "upscaler", ustr);
    push("Graphics", "antialias", g_settings.antialias == AA_SMAA ? "smaa" : "off");
    push("Graphics", "frame_gen", FormatBoolVal(eff_fg));
    push("Graphics", "fsr_jitter", FormatBoolVal(eff_jit));
    push("Graphics", "fsr_sharpness", FormatCompactDouble(eff_shp));
    push("Graphics", "lod_bias", FormatCompactDouble(eff_lb));
    push("Graphics", "lod_min", FormatCompactDouble(eff_lm));
    push("Graphics", "no_shadow", FormatBoolVal(eff_ns));

    const int eff_vol = g_extra_audio_volume >= 0 ? g_extra_audio_volume
                      : (EnvNonEmpty("AC_MASTER_VOLUME") ? std::atoi(EnvStr("AC_MASTER_VOLUME")) : kDefaultAudioVolume);
    const bool eff_mute = (g_extra_audio_mute_unfocus >= 0) ? (g_extra_audio_mute_unfocus != 0)
                        : (EnvNonEmpty("AC_MUTE_ON_UNFOCUS") && ParseBoolVal(EnvStr("AC_MUTE_ON_UNFOCUS"), false));
    push("Audio", "master_volume", std::to_string(std::clamp(eff_vol, 0, EXTRA_AUDIO_VOL_MAX)));
    push("Audio", "mute_on_unfocus", FormatBoolVal(eff_mute));

    const char* curve_str = std::fabs(g_extra_mouse_curve_exp - kCurveLinearExp) < kCurveMatchEps ? "linear"
                          : std::fabs(g_extra_mouse_curve_exp - kCurveAggressiveExp) < kCurveMatchEps ? "aggressive"
                          : "power";
    push("Mouse", "mouse_look", FormatBoolVal(g_extra_mouse_look != 0));
    push("Mouse", "sensitivity", FormatCompactDouble(g_extra_mouse_sens * kMouseSensPercentDiv));
    push("Mouse", "y_scale", FormatCompactDouble(g_extra_mouse_yscale));
    push("Mouse", "curve", curve_str);
    push("Mouse", "invert_y", FormatBoolVal(g_extra_mouse_invert != 0));
    push("Mouse", "menu_nav", FormatBoolVal(g_extra_mouse_menu_nav != 0));
    push("Mouse", "menu_motion", FormatBoolVal(g_extra_mouse_menu_motion != 0));
    push("Mouse", "menu_cell", std::to_string(static_cast<int>(std::lround(g_extra_mouse_menu_cell))));

    const int lidx = (g_settings.language >= 0 && g_settings.language < N_LANGUAGES) ? g_settings.language : 0;
    const char* vlang = g_settings.voice_language == 1 ? "ja" : (g_settings.voice_language == 2 ? "en" : "auto");
    const char* pm_str = g_extra_prompts_override == EXTRA_PROMPT_MODE_XBOX ? "xbox"
                       : g_extra_prompts_override == EXTRA_PROMPT_MODE_SWITCH ? "switch"
                       : g_extra_prompts_override == EXTRA_PROMPT_MODE_KBM ? "keyboard" : "auto";
    const bool eff_igf = g_extra_ignore_focus >= 0 ? (g_extra_ignore_focus != 0) : EnvNonEmpty("AC_IGNORE_FOCUS");
    const bool eff_c30 = g_extra_cutscene_30fps >= 0 ? (g_extra_cutscene_30fps != 0)
                       : (EnvNonEmpty("AC_CUTSCENE_FPS") && std::atoi(EnvStr("AC_CUTSCENE_FPS")) == kCutsceneBaseFps);
    const bool eff_xca = g_extra_gamepad_confirm_a >= 0 ? (g_extra_gamepad_confirm_a != 0)
                       : (!EnvNonEmpty("AC_XBOX_CONFIRM_A") || EnvStr("AC_XBOX_CONFIRM_A")[0] != '0');

    push("Game", "language", g_language_codes[lidx]);
    push("Game", "voice_language", vlang);
    push("Game", "button_prompts", pm_str);
    push("Game", "xbox_confirm_a", FormatBoolVal(eff_xca));
    push("Game", "ignore_focus", FormatBoolVal(eff_igf));
    push("Game", "cutscene_30fps", FormatBoolVal(eff_c30));
    if (st.boot_async_env || g_extra_async_kill) {
        push("Game", "async_submit", FormatBoolVal(!g_extra_async_kill));
    }

    AcPerfHudConfig hud{};
    ac_perf_hud_get_config(&hud);
    push("Overlay", "perf_hud_enabled", FormatBoolVal(hud.enabled));
    push("Overlay", "perf_hud_mode", std::to_string(hud.mode));
    push("Overlay", "perf_hud_corner", std::to_string(hud.corner));
    push("Overlay", "perf_hud_opacity", FormatCompactDouble(hud.opacity));
    push("Overlay", "perf_hud_show_with_menu", FormatBoolVal(hud.show_with_menu));

    const auto& rpc = AcDiscordRpc::GetSettings();
    push("DiscordRPC", "enabled", FormatBoolVal(rpc.enabled.load(std::memory_order_relaxed)));
    push("DiscordRPC", "show_file", FormatBoolVal(rpc.show_file.load(std::memory_order_relaxed)));
    push("DiscordRPC", "show_mission", FormatBoolVal(rpc.show_mission.load(std::memory_order_relaxed)));
    push("DiscordRPC", "show_area", FormatBoolVal(rpc.show_area.load(std::memory_order_relaxed)));
    push("DiscordRPC", "show_boss", FormatBoolVal(rpc.show_boss.load(std::memory_order_relaxed)));
    push("DiscordRPC", "show_difficulty", FormatBoolVal(rpc.show_difficulty.load(std::memory_order_relaxed)));
    push("DiscordRPC", "show_time", FormatBoolVal(rpc.show_time.load(std::memory_order_relaxed)));
    push("DiscordRPC", "client_id", AcDiscordRpc::GetClientId());
    return out;
}

} // namespace ac::config
