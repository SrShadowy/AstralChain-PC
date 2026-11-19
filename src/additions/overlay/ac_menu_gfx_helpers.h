// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Constants and effective-state helpers for the Graphics / Audio / Mouse menu tabs.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "settings.h"
#include "additions/toggles/ac_toggles.h"

namespace ac::menu {

constexpr int kDefaultVolumePct      = 100;
constexpr float kMinSensPct          = 25.0f;
constexpr float kMaxSensPct          = 4000.0f;
constexpr float kSliderMaxSensPct    = 1000.0f;
constexpr float kMinYScale           = 0.20f;
constexpr float kMaxYScale           = 10.0f;
constexpr float kSliderMaxYScale     = 3.0f;
constexpr float kMinMenuCellPx       = 10.0f;
constexpr float kMaxMenuCellPx       = 200.0f;
constexpr double kCurveLinear        = 1.0;
constexpr double kCurvePower         = 1.35;
constexpr double kCurveAggressive    = 1.75;
constexpr double kCurveMatchEps      = 0.02;
constexpr int kDefaultWinWidth       = 1600;
constexpr int kDefaultWinHeight      = 900;
constexpr int kResPresetCount        = 5;
constexpr int kOutResPresetCount     = 6;
constexpr int kAspectPresetCount     = 5;
constexpr int kShadowScaleCount      = 3;
constexpr int kUpscalerModeCount     = 3;
constexpr int kScalePresetCount      = 9;
constexpr int kDefaultScaleIndex     = 4;
constexpr int kCustomResBufSize      = 48;
constexpr int kScaleLabelBufSize     = 56;
constexpr int kAntialiasCount        = 2;
constexpr float kMaxLodBiasSlider    = 3.0f;
constexpr float kMaxLodMinSlider     = 4.0f;
constexpr float kMaxSharpnessSlider  = 1.0f;

struct ResPreset {
    int w;
    int h;
    const char *label;
};

constexpr ResPreset kResPresets[kResPresetCount] = {
    {1280, 720, "1280x720 (HD)"},
    {1600, 900, "1600x900 (HD+ Padrao)"},
    {1920, 1080, "1920x1080 (Full HD)"},
    {2560, 1440, "2560x1440 (QHD)"},
    {3840, 2160, "3840x2160 (4K UHD)"}
};

constexpr const char *const kOutResKeys[kOutResPresetCount] = {
    "auto", "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"
};
constexpr const char *const kOutResLabels[kOutResPresetCount] = {
    "Auto (Acompanhar Janela) [Reinício]",
    "1280x720 (HD 720p) [Reinício]",
    "1600x900 (Switch Docked Nativo) [Reinício]",
    "1920x1080 (Full HD) [Reinício]",
    "2560x1440 (QHD) [Reinício]",
    "3840x2160 (4K UHD + Heaps 4K) [Reinício]"
};

constexpr const char *const kAspectKeys[kAspectPresetCount] = {
    "auto", "16:9", "16:10", "21:9", "32:9"
};
constexpr const char *const kAspectLabels[kAspectPresetCount] = {
    "Auto (Janela) [Reinício]",
    "16:9 (Padrão) [Reinício]",
    "16:10 (Steam Deck) [Reinício]",
    "21:9 (Ultrawide) [Reinício]",
    "32:9 (Super Ultrawide) [Reinício]"
};

constexpr int kShadowScaleVals[kShadowScaleCount] = {1, 2, 4};
constexpr const char *const kShadowScaleLabels[kShadowScaleCount] = {
    "1x (2048x2048 Nativo) [Reinício]",
    "2x (4096x4096 Nítido) [Reinício]",
    "4x (8192x8192 Ultra) [Reinício]"
};

constexpr int kUpscalerValues[kUpscalerModeCount] = {UPSCALE_BILINEAR, UPSCALE_FSR1, UPSCALE_FSR3};
constexpr const char *const kUpscalerLabels[kUpscalerModeCount] = {
    "Off (Bilinear)",
    "AMD FSR 1.0 (Espacial)",
    "AMD FSR 3.1 (Temporal + Jitter + RCAS)"
};
constexpr const char *const kUpscalerPrefixes[kUpscalerModeCount] = {"", "FSR 1.0 ", "FSR 3.1 "};
constexpr const char *const kNativeScaleTags[kUpscalerModeCount] = {"Nativo 1:1", "FSR 1.0 Nativo", "FSR 3.1 Native AA"};
constexpr float kScaleValues[kScalePresetCount] = {0.50f, 0.58f, 0.67f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f};
constexpr const char *const kScaleTags[kScalePresetCount] = {
    "Scene Perf", "Scene Bal", "Scene Qual", "Scene UQ",
    nullptr,
    "SSAA Qual", "SSAA Ultra", "SSAA Ext", "SSAA 4K"
};
constexpr const char *const kAntialiasLabels[kAntialiasCount] = {"Desativado", "SMAA Ultra (Color Edge)"};

inline int eff_output_res_index(char *custom_label, size_t custom_len) {
    const char *v = nullptr;
    if (g_extra_output_res[0]) {
        v = g_extra_output_res;
    } else {
        const char *e = std::getenv("AC_OUTPUT_RES");
        if (!e || !*e) e = std::getenv("AC_RESOLUTION");
        v = (e && *e) ? e : "1600x900";
    }
    for (int i = 0; i < kOutResPresetCount; i++) {
        if (std::strcmp(v, kOutResKeys[i]) == 0) return i;
    }
    std::snprintf(custom_label, custom_len, "%s [Reinício]", v);
    return -1;
}

inline int eff_aspect_index(void) {
    const char *v = nullptr;
    if (g_extra_aspect_str[0]) {
        v = g_extra_aspect_str;
    } else {
        const char *e = std::getenv("AC_ASPECT");
        v = (e && *e) ? e : "auto";
    }
    for (int i = 0; i < kAspectPresetCount; i++) {
        if (std::strcmp(v, kAspectKeys[i]) == 0) return i;
    }
    return 0;
}

inline int eff_draw_distance(void) {
    if (g_extra_draw_distance >= EXTRA_DRAW_DISTANCE_MIN) {
        return std::clamp(g_extra_draw_distance, EXTRA_DRAW_DISTANCE_MIN, EXTRA_DRAW_DISTANCE_MAX);
    }
    const char *e = std::getenv("AC_DRAW_DISTANCE");
    int v = (e && *e) ? std::atoi(e) : EXTRA_DRAW_DISTANCE_DEFAULT;
    return std::clamp(v, EXTRA_DRAW_DISTANCE_MIN, EXTRA_DRAW_DISTANCE_MAX);
}

inline int eff_shadow_scale_index(void) {
    int v = g_extra_shadow_scale;
    if (v < EXTRA_SHADOW_SCALE_MIN) {
        const char *e = std::getenv("AC_SHADOW_SCALE");
        v = (e && *e) ? std::atoi(e) : EXTRA_SHADOW_SCALE_DEFAULT;
    }
    for (int i = 0; i < kShadowScaleCount; i++) {
        if (kShadowScaleVals[i] == v) return i;
    }
    return 0;
}

inline bool eff_hud_hidden(void) {
    if (g_extra_no_hud >= 0) return g_extra_no_hud != 0;
    const char *e = std::getenv("AC_HUD");
    return e && *e && std::atoi(e) == 0;
}

inline bool eff_hud_centered(void) {
    if (g_extra_hud_area >= 0) return g_extra_hud_area == EXTRA_HUD_AREA_16_9;
    const char *e = std::getenv("AC_HUD_AREA");
    return !(e && *e && std::strcmp(e, "screen") == 0);
}

inline float eff_lod_bias(void) {
    if (g_extra_lod_bias >= 0.0f) return g_extra_lod_bias;
    const char *e = std::getenv("LARECOMP_LOD_BIAS");
    return e ? static_cast<float>(std::atof(e)) : 0.0f;
}

inline float eff_lod_min(void) {
    if (g_extra_lod_min >= 0.0f) return g_extra_lod_min;
    const char *e = std::getenv("LARECOMP_LOD_MIN");
    return e ? static_cast<float>(std::atof(e)) : 0.0f;
}

inline bool eff_no_shadow(void) {
    if (g_extra_no_shadow >= 0) return g_extra_no_shadow != 0;
    return std::getenv("LARECOMP_NO_SHADOW") != nullptr;
}

inline int eff_audio_volume(void) {
    if (g_extra_audio_volume >= 0) return std::clamp(g_extra_audio_volume, 0, EXTRA_AUDIO_VOL_MAX);
    const char *e = std::getenv("AC_MASTER_VOLUME");
    return (e && *e) ? std::clamp(std::atoi(e), 0, EXTRA_AUDIO_VOL_MAX) : kDefaultVolumePct;
}

inline bool eff_audio_mute_unfocus(void) {
    if (g_extra_audio_mute_unfocus >= 0) return g_extra_audio_mute_unfocus != 0;
    const char *e = std::getenv("AC_MUTE_ON_UNFOCUS");
    return e && (*e == '1' || std::strcmp(e, "true") == 0);
}

} // namespace ac::menu
