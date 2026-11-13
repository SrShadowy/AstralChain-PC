// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Section-level INI parsing and Graphics/Audio/Mouse appliers.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "settings.h"
#include "additions/config/ac_ini_store.h"
#include "additions/config/ac_key_map.h"
#include "additions/toggles/ac_toggles.h"

namespace ac::config {

constexpr int kDefaultWinWidth         = 1600;
constexpr int kDefaultWinHeight        = 900;
constexpr int kMinWinDim               = 320;
constexpr int kMaxWinDim               = 7680;
constexpr int kDefaultFpsLimit         = 60;
constexpr int kMaxFpsLimit             = 1000;
constexpr float kMinRenderScale        = 0.50f;
constexpr float kMaxRenderScale        = 2.00f;
constexpr float kMinFsrSharpness       = 0.00f;
constexpr float kMaxFsrSharpness       = 1.00f;
constexpr float kDefaultFsrSharpness   = 0.80f;
constexpr float kMaxLodBias            = 3.00f;
constexpr float kMaxLodMin             = 4.00f;
constexpr int kDefaultAudioVolume      = 100;
constexpr double kMouseSensPercentDiv  = 100.0;
constexpr double kDefaultMouseSensPct  = 100.0;
constexpr double kMinMouseSensPct      = 5.0;
constexpr double kMaxMouseSensPct      = 4000.0;
constexpr double kMinMouseYScale       = 0.05;
constexpr double kMaxMouseYScale       = 10.0;
constexpr double kCurveLinearExp       = 1.0;
constexpr double kCurvePowerExp        = 1.35;
constexpr double kCurveAggressiveExp   = 1.75;
constexpr double kMinMouseCurve        = 0.2;
constexpr double kMaxMouseCurve        = 5.0;
constexpr double kCurveMatchEps        = 0.02;
constexpr float kMinMenuCell           = 10.0f;
constexpr float kMaxMenuCell           = 200.0f;
constexpr int kCutsceneBaseFps         = 30;
constexpr int kPerfHudLegacyCompact    = 1;
constexpr int kPerfHudLegacyDetailed   = 2;
constexpr int kPerfHudLegacyMinimal    = 3;
constexpr const char* kDefaultRpcId    = "1568955734606086196";
constexpr const char* kPmMailbox       = "mailbox";
constexpr const char* kPmFifo          = "fifo";
constexpr const char* kPmFifoRelaxed   = "fifo_relaxed";
constexpr const char* kPmImmediate     = "immediate";

struct SyncState {
    int win_w = kDefaultWinWidth;
    int win_h = kDefaultWinHeight;
    bool boot_async_env = false;
};

inline bool EnvNonEmpty(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && *v != '\0';
}

inline const char* EnvStr(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : nullptr;
}

inline const std::string* FindKey(const std::vector<IniEntry>& entries,
                                  const std::string& section,
                                  const std::string& key,
                                  bool allow_global_fallback) {
    const std::string* v = FindIniValue(entries, section, key);
    if (!v && allow_global_fallback) {
        v = FindIniValue(entries, "", key);
    }
    return v;
}

inline bool ParseBoolVal(const std::string& s, bool fallback) {
    if (EqualsIgnoreCase(s, "true") || EqualsIgnoreCase(s, "on") ||
        EqualsIgnoreCase(s, "yes") || s == "1") {
        return true;
    }
    if (EqualsIgnoreCase(s, "false") || EqualsIgnoreCase(s, "off") ||
        EqualsIgnoreCase(s, "no") || s == "0") {
        return false;
    }
    char* end = nullptr;
    long n = std::strtol(s.c_str(), &end, 10);
    return (end != s.c_str()) ? (n != 0) : fallback;
}

inline int ParseIntVal(const std::string& s, int fallback) {
    char* end = nullptr;
    long n = std::strtol(s.c_str(), &end, 10);
    return (end != s.c_str() && *end == '\0') ? static_cast<int>(n) : fallback;
}

inline double ParseDoubleVal(const std::string& s, double fallback) {
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    return (end != s.c_str() && std::isfinite(d)) ? d : fallback;
}

inline const char* CanonicalPresentMode(const char* raw) {
    if (!raw || !*raw) {
        return kPmMailbox;
    }
    if (std::strcmp(raw, "fifo") == 0) {
        return kPmFifo;
    }
    if (std::strcmp(raw, "immediate") == 0) {
        return kPmImmediate;
    }
    if (std::strcmp(raw, "fifo_relaxed") == 0 || std::strcmp(raw, "fifo-relaxed") == 0) {
        return kPmFifoRelaxed;
    }
    return kPmMailbox;
}

inline std::string FormatCompactDouble(double v) {
    double rounded = std::round(v * 100.0) / 100.0;
    if (std::fabs(rounded) < 1e-6) {
        rounded = 0.0;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4g", rounded);
    return buf;
}

inline std::string FormatBoolVal(bool v) {
    return v ? "true" : "false";
}

inline double ParseMouseCurve(const char* raw) {
    if (!raw || !*raw) {
        return kCurvePowerExp;
    }
    if (std::strcmp(raw, "linear") == 0 || std::strcmp(raw, "1") == 0) {
        return kCurveLinearExp;
    }
    if (std::strcmp(raw, "power") == 0) {
        return kCurvePowerExp;
    }
    if (std::strcmp(raw, "aggressive") == 0 || std::strcmp(raw, "2") == 0) {
        return kCurveAggressiveExp;
    }
    double d = ParseDoubleVal(raw, kCurvePowerExp);
    return std::clamp(d, kMinMouseCurve, kMaxMouseCurve);
}

inline void ApplyGraphicsSection(const std::vector<IniEntry>& entries,
                                 bool boot,
                                 bool legacy_fallback,
                                 SyncState& st) {
    const std::string* vw = FindKey(entries, "Graphics", "width", false);
    const std::string* vh = FindKey(entries, "Graphics", "height", false);
    if (boot) {
        int ew = EnvNonEmpty("AC_WIDTH") ? std::atoi(EnvStr("AC_WIDTH")) : 0;
        int eh = EnvNonEmpty("AC_HEIGHT") ? std::atoi(EnvStr("AC_HEIGHT")) : 0;
        int iw = vw ? std::clamp(ParseIntVal(*vw, kDefaultWinWidth), kMinWinDim, kMaxWinDim) : kDefaultWinWidth;
        int ih = vh ? std::clamp(ParseIntVal(*vh, kDefaultWinHeight), kMinWinDim, kMaxWinDim) : kDefaultWinHeight;
        st.win_w = (ew >= kMinWinDim && ew <= kMaxWinDim) ? ew : iw;
        st.win_h = (eh >= kMinWinDim && eh <= kMaxWinDim) ? eh : ih;
        g_extra_win_w = st.win_w;
        g_extra_win_h = st.win_h;
    } else if (vw && vh) {
        int iw = std::clamp(ParseIntVal(*vw, st.win_w), kMinWinDim, kMaxWinDim);
        int ih = std::clamp(ParseIntVal(*vh, st.win_h), kMinWinDim, kMaxWinDim);
        if (iw != st.win_w || ih != st.win_h) {
            st.win_w = iw;
            st.win_h = ih;
            g_extra_win_w = iw;
            g_extra_win_h = ih;
            g_extra_win_resize_pending = 1;
        }
    }

    if (const std::string* vfs = FindKey(entries, "Graphics", "fullscreen", false)) {
        bool want_fs = ParseBoolVal(*vfs, false);
        bool cur_fs = (g_extra_fullscreen_pending >= 0 ? g_extra_fullscreen_pending : g_sdl_fullscreen) != 0;
        if (boot) {
            if (!EnvNonEmpty("AC_FULLSCREEN")) {
                g_extra_fullscreen_pending = want_fs ? 1 : 0;
            }
        } else if (want_fs != cur_fs) {
            g_extra_fullscreen_pending = want_fs ? 1 : 0;
        }
    }

    const std::string* vpm = FindKey(entries, "Graphics", "present_mode", false);
    const std::string* vvs = FindKey(entries, "Graphics", "vsync", false);
    if ((vpm || vvs) && (!boot || !EnvNonEmpty("LARECOMP_VK_PRESENT"))) {
        const char* want_pm = vpm ? CanonicalPresentMode(vpm->c_str())
                                  : (ParseBoolVal(*vvs, true) ? kPmFifo : kPmMailbox);
        const char* cur_pm = CanonicalPresentMode(g_extra_present_mode ? g_extra_present_mode : EnvStr("LARECOMP_VK_PRESENT"));
        if (std::strcmp(want_pm, cur_pm) != 0 || (boot && vpm)) {
            g_extra_present_mode = want_pm;
            if (!boot) {
                g_extra_present_pending = 1;
            }
        }
    }

    if (const std::string* vfps = FindKey(entries, "Graphics", "fps_limit", false)) {
        if (!boot || (!EnvNonEmpty("AC_FPS") && !EnvNonEmpty("AC_TARGET_FPS"))) {
            g_extra_fps_override = std::clamp(ParseIntVal(*vfps, kDefaultFpsLimit), 0, kMaxFpsLimit);
        }
    }
    if (const std::string* vup = FindKey(entries, "Graphics", "upscaler", legacy_fallback)) {
        if (!boot || !EnvNonEmpty("AC_UPSCALER")) {
            g_settings.upscaler = (*vup == "fsr3") ? UPSCALE_FSR3 : (*vup == "fsr1" ? UPSCALE_FSR1 : UPSCALE_BILINEAR);
        }
    }
    if (const std::string* vaa = FindKey(entries, "Graphics", "antialias", legacy_fallback)) {
        if (!boot || !EnvNonEmpty("AC_ANTIALIAS")) {
            g_settings.antialias = (*vaa == "off" || *vaa == "0") ? AA_OFF : AA_SMAA;
        }
    }
    if (const std::string* vrsc = FindKey(entries, "Graphics", "render_scale", legacy_fallback)) {
        if (!boot || !EnvNonEmpty("AC_RENDER_SCALE")) {
            float rs = std::clamp(static_cast<float>(ParseDoubleVal(*vrsc, 1.0)), kMinRenderScale, kMaxRenderScale);
            g_settings.render_scale = rs;
            g_extra_render_scale = rs;
        }
    }
    const std::string* vfg = FindKey(entries, "Graphics", "frame_gen", legacy_fallback);
    if (!vfg) {
        vfg = FindKey(entries, "Graphics", "framegen", legacy_fallback);
    }
    if (vfg && (!boot || !EnvNonEmpty("AC_FRAMEGEN"))) {
        int fg = ParseBoolVal(*vfg, false) ? 1 : 0;
        g_settings.framegen = fg;
        g_extra_framegen = fg;
    }
    if (const std::string* vjit = FindKey(entries, "Graphics", "fsr_jitter", legacy_fallback)) {
        if (!boot || !EnvNonEmpty("AC_FSR_JITTER")) {
            int j = ParseBoolVal(*vjit, true) ? 1 : 0;
            g_settings.fsr_jitter = j;
            g_extra_fsr_jitter = j;
        }
    }
    if (const std::string* vshp = FindKey(entries, "Graphics", "fsr_sharpness", legacy_fallback)) {
        if (!boot || !EnvNonEmpty("AC_FSR_SHARPNESS")) {
            float sh = std::clamp(static_cast<float>(ParseDoubleVal(*vshp, kDefaultFsrSharpness)),
                                  kMinFsrSharpness, kMaxFsrSharpness);
            g_settings.fsr_sharpness = sh;
            g_extra_fsr_sharpness = sh;
        }
    }
    if (const std::string* vlb = FindKey(entries, "Graphics", "lod_bias", false)) {
        if (!boot || !EnvNonEmpty("LARECOMP_LOD_BIAS")) {
            g_extra_lod_bias = std::clamp(static_cast<float>(ParseDoubleVal(*vlb, 0.0)), 0.0f, kMaxLodBias);
        }
    }
    if (const std::string* vlm = FindKey(entries, "Graphics", "lod_min", false)) {
        if (!boot || !EnvNonEmpty("LARECOMP_LOD_MIN")) {
            g_extra_lod_min = std::clamp(static_cast<float>(ParseDoubleVal(*vlm, 0.0)), 0.0f, kMaxLodMin);
        }
    }
    if (const std::string* vns = FindKey(entries, "Graphics", "no_shadow", false)) {
        if (!boot || !EnvNonEmpty("LARECOMP_NO_SHADOW")) {
            g_extra_no_shadow = ParseBoolVal(*vns, false) ? 1 : 0;
        }
    }
}

inline void ApplyAudioSection(const std::vector<IniEntry>& entries, bool boot) {
    bool changed = false;
    if (const std::string* vvol = FindKey(entries, "Audio", "master_volume", false)) {
        if (!boot || !EnvNonEmpty("AC_MASTER_VOLUME")) {
            int vol = std::clamp(ParseIntVal(*vvol, kDefaultAudioVolume), 0, EXTRA_AUDIO_VOL_MAX);
            if (g_extra_audio_volume != vol) {
                g_extra_audio_volume = vol;
                changed = true;
            }
        }
    }
    if (const std::string* vmute = FindKey(entries, "Audio", "mute_on_unfocus", false)) {
        if (!boot || !EnvNonEmpty("AC_MUTE_ON_UNFOCUS")) {
            int mute = ParseBoolVal(*vmute, false) ? 1 : 0;
            if (g_extra_audio_mute_unfocus != mute) {
                g_extra_audio_mute_unfocus = mute;
                changed = true;
            }
        }
    }
    if (changed) {
        g_extra_audio_gen++;
    }
}

inline void ApplyMouseSection(const std::vector<IniEntry>& entries, bool boot) {
    int look = g_extra_mouse_override ? g_extra_mouse_look : 1;
    double sens = g_extra_mouse_override ? g_extra_mouse_sens : 1.0;
    double yscale = g_extra_mouse_override ? g_extra_mouse_yscale : 1.0;
    double curve = g_extra_mouse_override ? g_extra_mouse_curve_exp : kCurvePowerExp;
    int invert = g_extra_mouse_override ? g_extra_mouse_invert : 0;
    int menu_nav = g_extra_mouse_override ? g_extra_mouse_menu_nav : 1;
    int menu_motion = g_extra_mouse_override ? g_extra_mouse_menu_motion : 1;
    float menu_cell = g_extra_mouse_override ? g_extra_mouse_menu_cell : EXTRA_MOUSE_CELL_DEFAULT;

    if (boot) {
        if (const char* e = EnvStr("AC_MOUSE_LOOK")) {
            look = (e[0] != '0' && std::strcmp(e, "false") != 0) ? 1 : 0;
        }
        if (const char* e = EnvStr("AC_MOUSE_SENS")) {
            sens = std::clamp(ParseDoubleVal(e, kDefaultMouseSensPct), kMinMouseSensPct, kMaxMouseSensPct) / kMouseSensPercentDiv;
        }
        if (const char* e = EnvStr("AC_MOUSE_YSCALE")) {
            yscale = std::clamp(ParseDoubleVal(e, 1.0), kMinMouseYScale, kMaxMouseYScale);
        }
        if (const char* e = EnvStr("AC_MOUSE_CURVE")) {
            curve = ParseMouseCurve(e);
        }
        if (const char* e = EnvStr("AC_MOUSE_INVERT")) {
            invert = (e[0] == '1' || std::strcmp(e, "true") == 0) ? 1 : 0;
        }
        if (const char* e = EnvStr("AC_MOUSE_MENUS")) {
            menu_nav = (e[0] != '0') ? 1 : 0;
        }
        if (const char* e = EnvStr("AC_MOUSE_MENU_MOTION")) {
            menu_motion = (e[0] != '0') ? 1 : 0;
        }
        if (const char* e = EnvStr("AC_MOUSE_MENU_CELL")) {
            menu_cell = std::clamp(static_cast<float>(ParseDoubleVal(e, EXTRA_MOUSE_CELL_DEFAULT)), kMinMenuCell, kMaxMenuCell);
        }
    }

    if (const std::string* v = FindKey(entries, "Mouse", "mouse_look", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_LOOK")) {
            look = ParseBoolVal(*v, true) ? 1 : 0;
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "sensitivity", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_SENS")) {
            sens = std::clamp(ParseDoubleVal(*v, kDefaultMouseSensPct), kMinMouseSensPct, kMaxMouseSensPct) / kMouseSensPercentDiv;
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "y_scale", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_YSCALE")) {
            yscale = std::clamp(ParseDoubleVal(*v, 1.0), kMinMouseYScale, kMaxMouseYScale);
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "curve", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_CURVE")) {
            curve = ParseMouseCurve(v->c_str());
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "invert_y", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_INVERT")) {
            invert = ParseBoolVal(*v, false) ? 1 : 0;
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "menu_nav", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_MENUS")) {
            menu_nav = ParseBoolVal(*v, true) ? 1 : 0;
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "menu_motion", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_MENU_MOTION")) {
            menu_motion = ParseBoolVal(*v, true) ? 1 : 0;
        }
    }
    if (const std::string* v = FindKey(entries, "Mouse", "menu_cell", false)) {
        if (!boot || !EnvNonEmpty("AC_MOUSE_MENU_CELL")) {
            menu_cell = std::clamp(static_cast<float>(ParseDoubleVal(*v, EXTRA_MOUSE_CELL_DEFAULT)), kMinMenuCell, kMaxMenuCell);
        }
    }

    const bool look_changed = (g_extra_mouse_look != look);
    const bool diff = !g_extra_mouse_override || look_changed ||
                      std::fabs(g_extra_mouse_sens - sens) > 1e-6 ||
                      std::fabs(g_extra_mouse_yscale - yscale) > 1e-6 ||
                      std::fabs(g_extra_mouse_curve_exp - curve) > 1e-6 ||
                      g_extra_mouse_invert != invert ||
                      g_extra_mouse_menu_nav != menu_nav ||
                      g_extra_mouse_menu_motion != menu_motion ||
                      std::fabs(g_extra_mouse_menu_cell - menu_cell) > 0.01f;
    g_extra_mouse_look = look;
    g_extra_mouse_sens = sens;
    g_extra_mouse_yscale = yscale;
    g_extra_mouse_curve_exp = curve;
    g_extra_mouse_invert = invert;
    g_extra_mouse_menu_nav = menu_nav;
    g_extra_mouse_menu_motion = menu_motion;
    g_extra_mouse_menu_cell = menu_cell;
    g_extra_mouse_override = 1;
    if (diff) {
        g_extra_mouse_gen++;
    }
    if (!boot && look_changed) {
        g_sdl_prompt_generation++;
    }
}

} // namespace ac::config
