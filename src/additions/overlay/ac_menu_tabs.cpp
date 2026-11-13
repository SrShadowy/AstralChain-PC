// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] In-game Audio and Mouse settings tab for the Legatus / IRIS terminal.
#include "additions/overlay/ac_menu_tabs.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "imgui.h"
#include "additions/overlay/ac_hud.h"
#include "additions/toggles/ac_toggles.h"

namespace {

constexpr int kDefaultVolumePct     = 100;
constexpr float kMinSensPct         = 5.0f;
constexpr float kMaxSensPct         = 4000.0f;
constexpr float kSliderMaxSensPct   = 400.0f;
constexpr float kMinYScale          = 0.05f;
constexpr float kMaxYScale          = 10.0f;
constexpr float kSliderMaxYScale    = 3.0f;
constexpr float kMinMenuCellPx      = 10.0f;
constexpr float kMaxMenuCellPx      = 200.0f;
constexpr double kCurveLinear       = 1.0;
constexpr double kCurvePower        = 1.35;
constexpr double kCurveAggressive   = 1.75;
constexpr double kCurveMatchEps     = 0.02;

int effective_audio_volume(void) {
    if (g_extra_audio_volume >= 0) {
        return std::clamp(g_extra_audio_volume, 0, EXTRA_AUDIO_VOL_MAX);
    }
    const char *e = std::getenv("AC_MASTER_VOLUME");
    if (e && *e) {
        return std::clamp(std::atoi(e), 0, EXTRA_AUDIO_VOL_MAX);
    }
    return kDefaultVolumePct;
}

bool effective_audio_mute_unfocus(void) {
    if (g_extra_audio_mute_unfocus >= 0) {
        return g_extra_audio_mute_unfocus != 0;
    }
    const char *e = std::getenv("AC_MUTE_ON_UNFOCUS");
    return e && (*e == '1' || std::strcmp(e, "true") == 0);
}

void ensure_mouse_initialized(void) {
    if (g_extra_mouse_override) {
        return;
    }
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
    if (std::fabs(exp - kCurveLinear) < kCurveMatchEps) {
        return 0;
    }
    if (std::fabs(exp - kCurveAggressive) < kCurveMatchEps) {
        return 2;
    }
    return 1;
}

double curve_index_to_exp(int idx) {
    if (idx == 0) {
        return kCurveLinear;
    }
    if (idx == 2) {
        return kCurveAggressive;
    }
    return kCurvePower;
}

void commit_audio_change(void) {
    g_extra_audio_gen++;
    ac_config_sync_save_hook();
}

void commit_mouse_change(bool bump_prompts) {
    g_extra_mouse_override = 1;
    g_extra_mouse_gen++;
    if (bump_prompts) {
        g_sdl_prompt_generation++;
    }
    ac_config_sync_save_hook();
}

} // namespace

void ac_menu_tab_audio_mouse(void) {
    ac_ui_section_header("ÁUDIO & MUTE AUTOMÁTICO", "// SYS.AUDIO");

    int vol = effective_audio_volume();
    if (ImGui::SliderInt("Volume Geral", &vol, 0, EXTRA_AUDIO_VOL_MAX, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_audio_volume = vol;
        commit_audio_change();
    }

    bool mute_unfocus = effective_audio_mute_unfocus();
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
        sens_pct = std::clamp(sens_pct, kMinSensPct, kMaxSensPct);
        g_extra_mouse_sens = static_cast<double>(sens_pct) / 100.0;
        commit_mouse_change(false);
    }

    float yscale = std::clamp(static_cast<float>(g_extra_mouse_yscale), kMinYScale, kMaxYScale);
    if (ImGui::SliderFloat("Escala Vertical (Y Scale)", &yscale, 0.25f, kSliderMaxYScale, "%.2fx")) {
        g_extra_mouse_yscale = std::clamp(static_cast<double>(yscale),
                                          static_cast<double>(kMinYScale),
                                          static_cast<double>(kMaxYScale));
        commit_mouse_change(false);
    }

    int curve_idx = curve_exp_to_index(g_extra_mouse_curve_exp);
    if (ImGui::Combo("Curva de Resposta", &curve_idx,
                     "Linear (1.0)\0Power Suave (1.35)\0Agressiva (1.75)\0")) {
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
    ImGui::TextDisabled("Alterações são aplicadas em tempo real e salvas em userdata/launcher.ini.");
}
