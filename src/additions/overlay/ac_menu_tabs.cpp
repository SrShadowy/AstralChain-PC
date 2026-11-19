// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] In-game Graphics, Audio and Mouse settings tabs for the Legatus / IRIS terminal.
#include "additions/overlay/ac_menu_tabs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "settings.h"
#include "additions/overlay/ac_hud.h"
#include "additions/overlay/ac_menu_gfx_helpers.h"
#include "additions/toggles/ac_toggles.h"

using namespace ac::menu;

namespace {

void ensure_mouse_initialized(void) {
    if (g_extra_mouse_override) return;
    const char *ml = std::getenv("AC_MOUSE_LOOK");
    g_extra_mouse_look = !(ml && *ml == '0') ? 1 : 0;
    g_extra_mouse_sens = 1.0;
    g_extra_mouse_yscale = 1.0;
    g_extra_mouse_curve_exp = kCurvePower;
    g_extra_mouse_invert = 0;
    g_extra_mouse_menu_nav = 1;
    g_extra_mouse_menu_motion = 1;
    g_extra_mouse_menu_cell = EXTRA_MOUSE_CELL_DEFAULT;
    g_extra_mouse_override = 1;
}

int curve_exp_to_index(double exp) {
    if (std::fabs(exp - kCurveLinear) < kCurveMatchEps) return 0;
    if (std::fabs(exp - kCurveAggressive) < kCurveMatchEps) return 2;
    return 1;
}

double curve_index_to_exp(int idx) {
    if (idx == 0) return kCurveLinear;
    if (idx == 2) return kCurveAggressive;
    return kCurvePower;
}

void commit_audio_change(void) {
    g_extra_audio_gen++;
    ac_config_sync_save_hook();
}

void commit_mouse_change(bool bump_prompts) {
    g_extra_mouse_override = 1;
    g_extra_mouse_gen++;
    if (bump_prompts) g_sdl_prompt_generation++;
    ac_config_sync_save_hook();
}

int upscaler_to_index(int upscaler) {
    for (int i = 0; i < kUpscalerModeCount; i++) {
        if (upscaler == kUpscalerValues[i]) return i;
    }
    return 0;
}

int find_nearest_scale_index(float scale) {
    int best_idx = kDefaultScaleIndex;
    float best_diff = std::fabs(scale - kScaleValues[kDefaultScaleIndex]);
    for (int i = 0; i < kScalePresetCount; i++) {
        float diff = std::fabs(scale - kScaleValues[i]);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }
    return best_idx;
}

void render_display_section(void) {
    ac_ui_section_header("JANELA, OUTPUT & PROPORÇÃO", "// SYS.DISP");
    int cur_w = g_extra_win_w > 0 ? g_extra_win_w : kDefaultWinWidth;
    int cur_h = g_extra_win_h > 0 ? g_extra_win_h : kDefaultWinHeight;
    int res_idx = kResPresetCount;
    for (int i = 0; i < kResPresetCount; i++) {
        if (kResPresets[i].w == cur_w && kResPresets[i].h == cur_h) {
            res_idx = i;
            break;
        }
    }
    char custom_buf[kCustomResBufSize];
    std::snprintf(custom_buf, sizeof(custom_buf), "Personalizada (%dx%d)", cur_w, cur_h);
    const char *preview = (res_idx < kResPresetCount) ? kResPresets[res_idx].label : custom_buf;
    if (ImGui::BeginCombo("Resolução da Janela", preview)) {
        for (int i = 0; i < kResPresetCount; i++) {
            bool selected = (i == res_idx);
            if (ImGui::Selectable(kResPresets[i].label, selected)) {
                g_extra_win_w = kResPresets[i].w;
                g_extra_win_h = kResPresets[i].h;
                g_extra_win_resize_pending = 1;
                ac_config_sync_save_hook();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    char custom_out_buf[kCustomResBufSize];
    int out_idx = eff_output_res_index(custom_out_buf, sizeof(custom_out_buf));
    const char *out_preview = (out_idx >= 0) ? kOutResLabels[out_idx] : custom_out_buf;
    if (ImGui::BeginCombo("Resolução Output (HUD/Composite)", out_preview)) {
        for (int i = 0; i < kOutResPresetCount; i++) {
            bool selected = (i == out_idx);
            if (ImGui::Selectable(kOutResLabels[i], selected)) {
                std::snprintf(g_extra_output_res, sizeof(g_extra_output_res), "%s", kOutResKeys[i]);
                ac_config_sync_save_hook();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    int asp_idx = eff_aspect_index();
    if (ImGui::Combo("Proporção (Aspect Ratio)", &asp_idx, kAspectLabels, kAspectPresetCount)) {
        std::snprintf(g_extra_aspect_str, sizeof(g_extra_aspect_str), "%s", kAspectKeys[asp_idx]);
        ac_config_sync_save_hook();
    }

    bool fs = (g_extra_fullscreen_pending >= 0 ? g_extra_fullscreen_pending : g_sdl_fullscreen) != 0;
    if (ImGui::Checkbox("Tela Cheia (F11)", &fs)) {
        g_extra_fullscreen_pending = fs ? 1 : 0;
        ac_config_sync_save_hook();
    }
}

void render_fsr_section(void) {
    ac_ui_section_header("SCENE RES, RECONSTRUÇÃO & FSR", "// SYS.FSR");
    int up = upscaler_to_index(g_settings.upscaler);
    if (ImGui::Combo("Upscaler", &up, kUpscalerLabels, kUpscalerModeCount)) {
        g_settings.upscaler = kUpscalerValues[up];
        settings_save();
    }

    float cur_sc = g_extra_render_scale > 0.0f ? g_extra_render_scale : g_settings.render_scale;
    if (cur_sc <= 0.0f) cur_sc = kScaleValues[kDefaultScaleIndex];
    int sc_idx = find_nearest_scale_index(cur_sc);
    char scale_bufs[kScalePresetCount][kScaleLabelBufSize];
    const char *scale_items[kScalePresetCount];
    for (int i = 0; i < kScalePresetCount; i++) {
        const char *tag = (i == kDefaultScaleIndex) ? kNativeScaleTags[up] : kScaleTags[i];
        const char *pfx = (i == kDefaultScaleIndex) ? "" : kUpscalerPrefixes[up];
        const char *suf = (i < kDefaultScaleIndex) ? " [Reinício]" : "";
        std::snprintf(scale_bufs[i], sizeof(scale_bufs[i]), "%.2fx (%s%s)%s", kScaleValues[i], pfx, tag, suf);
        scale_items[i] = scale_bufs[i];
    }
    if (ImGui::Combo("Escala Scene / SSAA", &sc_idx, scale_items, kScalePresetCount)) {
        g_extra_render_scale = kScaleValues[sc_idx];
        g_settings.render_scale = kScaleValues[sc_idx];
        settings_save();
    }

    bool fg = (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) != 0;
    if (ImGui::Checkbox("FSR 3.1 Frame Generation (Interpolação 2x FPS)", &fg)) {
        g_extra_framegen = fg ? 1 : 0;
        g_settings.framegen = g_extra_framegen;
        settings_save();
    }
    bool jit = (g_extra_fsr_jitter >= 0 ? g_extra_fsr_jitter : g_settings.fsr_jitter) != 0;
    if (ImGui::Checkbox("Camera Jitter Sub-Pixel (Halton 2,3)", &jit)) {
        g_extra_fsr_jitter = jit ? 1 : 0;
        g_settings.fsr_jitter = g_extra_fsr_jitter;
        settings_save();
    }
    float shp = g_extra_fsr_sharpness >= 0.0f ? g_extra_fsr_sharpness : g_settings.fsr_sharpness;
    if (ImGui::SliderFloat("Nitidez FSR (RCAS)", &shp, 0.0f, kMaxSharpnessSlider, "%.2f",
                           ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_fsr_sharpness = shp;
        g_settings.fsr_sharpness = shp;
        settings_save();
    }
}

void render_quality_section(void) {
    ac_ui_section_header("QUALIDADE, DRAW DISTANCE & HUD", "// SYS.GFX");
    int aa = std::clamp(g_settings.antialias, 0, kAntialiasCount - 1);
    if (ImGui::Combo("Antialiasing", &aa, kAntialiasLabels, kAntialiasCount)) {
        g_settings.antialias = aa;
        settings_save();
    }

    int dd = eff_draw_distance();
    if (ImGui::SliderInt("Distância de Visão (Ao vivo)", &dd,
                         EXTRA_DRAW_DISTANCE_MIN, EXTRA_DRAW_DISTANCE_MAX,
                         "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_draw_distance = dd;
        ac_config_sync_save_hook();
    }

    int ss_idx = eff_shadow_scale_index();
    if (ImGui::Combo("Sombras Solares (Shadow Scale)", &ss_idx, kShadowScaleLabels, kShadowScaleCount)) {
        g_extra_shadow_scale = kShadowScaleVals[ss_idx];
        ac_config_sync_save_hook();
    }

    float bias = eff_lod_bias();
    if (ImGui::SliderFloat("Texturas (Bias LOD)", &bias, 0.0f, kMaxLodBiasSlider, "%.1f",
                           ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_bias = bias;
        ac_config_sync_save_hook();
    }
    float lod = eff_lod_min();
    if (ImGui::SliderFloat("LoD Mínimo", &lod, 0.0f, kMaxLodMinSlider, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_min = lod;
        ac_config_sync_save_hook();
    }
    bool shadow = !eff_no_shadow();
    if (ImGui::Checkbox("Sombras Dinâmicas", &shadow)) {
        g_extra_no_shadow = shadow ? 0 : 1;
        ac_config_sync_save_hook();
    }
    bool hide_hud = eff_hud_hidden();
    if (ImGui::Checkbox("Ocultar HUD em Jogo (F12 - Ao vivo)", &hide_hud)) {
        g_extra_no_hud = hide_hud ? 1 : 0;
        ac_config_sync_save_hook();
    }
    bool hud_center = eff_hud_centered();
    if (ImGui::Checkbox("Centralizar HUD em 16:9 (Ultrawide - Ao vivo)", &hud_center)) {
        g_extra_hud_area = hud_center ? EXTRA_HUD_AREA_16_9 : EXTRA_HUD_AREA_SCREEN;
        ac_config_sync_save_hook();
    }
    ImGui::TextDisabled("Sincronizado bidirecionalmente com userdata/launcher.ini (Launcher Qt6).");
}

} // namespace

void ac_menu_tab_graphics(void) {
    render_display_section();
    ImGui::Spacing();
    render_fsr_section();
    ImGui::Spacing();
    render_quality_section();
}

void ac_menu_tab_audio_mouse(void) {
    ac_ui_section_header("ÁUDIO & MUTE AUTOMÁTICO", "// SYS.AUDIO");
    int vol = eff_audio_volume();
    if (ImGui::SliderInt("Volume Geral", &vol, 0, EXTRA_AUDIO_VOL_MAX, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_audio_volume = vol;
        commit_audio_change();
    }
    bool mute_unfocus = eff_audio_mute_unfocus();
    if (ImGui::Checkbox("Silenciar Quando a Janela Perder Foco", &mute_unfocus)) {
        g_extra_audio_mute_unfocus = mute_unfocus ? 1 : 0;
        commit_audio_change();
    }

    ImGui::Spacing();
    ac_ui_section_header("CÂMERA COM MOUSE (MOUSE LOOK)", "// SYS.MOUSE");
    ensure_mouse_initialized();

    bool look = g_extra_mouse_look != 0;
    if (ImGui::Checkbox("Controlar Câmera com Mouse (F10 Captura/Solta)", &look)) {
        g_extra_mouse_look = look ? 1 : 0;
        commit_mouse_change(true);
    }
    float sens_pct = std::clamp(static_cast<float>(g_extra_mouse_sens * 100.0), kMinSensPct, kMaxSensPct);
    if (ImGui::SliderFloat("Sensibilidade (%)", &sens_pct, 25.0f, kSliderMaxSensPct, "%.0f%%")) {
        g_extra_mouse_sens = static_cast<double>(std::clamp(sens_pct, kMinSensPct, kMaxSensPct)) / 100.0;
        commit_mouse_change(false);
    }
    float yscale = std::clamp(static_cast<float>(g_extra_mouse_yscale), kMinYScale, kMaxYScale);
    if (ImGui::SliderFloat("Escala Vertical (Y Scale)", &yscale, 0.25f, kSliderMaxYScale, "%.2fx")) {
        g_extra_mouse_yscale = static_cast<double>(std::clamp(yscale, kMinYScale, kMaxYScale));
        commit_mouse_change(false);
    }
    int curve_idx = curve_exp_to_index(g_extra_mouse_curve_exp);
    if (ImGui::Combo("Curva de Resposta", &curve_idx, "Linear (1.0)\0Power Suave (1.35)\0Agressiva (1.75)\0")) {
        g_extra_mouse_curve_exp = curve_index_to_exp(curve_idx);
        commit_mouse_change(false);
    }
    bool invert_y = g_extra_mouse_invert != 0;
    if (ImGui::Checkbox("Inverter Eixo Y da Câmera", &invert_y)) {
        g_extra_mouse_invert = invert_y ? 1 : 0;
        commit_mouse_change(false);
    }

    ImGui::Spacing();
    ac_ui_section_header("NAVEGAÇÃO EM MENUS COM PONTEIRO", "// SYS.MENU");
    bool menu_nav = g_extra_mouse_menu_nav != 0;
    if (ImGui::Checkbox("Navegação por Mouse nos Menus do Jogo", &menu_nav)) {
        g_extra_mouse_menu_nav = menu_nav ? 1 : 0;
        commit_mouse_change(false);
    }
    ImGui::BeginDisabled(!menu_nav);
    bool menu_motion = g_extra_mouse_menu_motion != 0;
    if (ImGui::Checkbox("Movimento do Ponteiro Navega Listas", &menu_motion)) {
        g_extra_mouse_menu_motion = menu_motion ? 1 : 0;
        commit_mouse_change(false);
    }
    float cell_px = std::clamp(g_extra_mouse_menu_cell, kMinMenuCellPx, kMaxMenuCellPx);
    if (ImGui::SliderFloat("Distância por Passo (px)", &cell_px, kMinMenuCellPx, kMaxMenuCellPx,
                           "%.0f px", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_mouse_menu_cell = cell_px;
        commit_mouse_change(false);
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Alterações são aplicadas em tempo real e salvas em userdata/launcher.ini.");
}
