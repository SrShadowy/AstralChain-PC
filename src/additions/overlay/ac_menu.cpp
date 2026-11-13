// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] In-game Legatus / IRIS dashboard (Insert): Performance / Graphics / Audio & Mouse / Game & RPC / Diagnostics.
// Writes directly onto runtime toggles (additions/toggles/ac_toggles.h) or g_settings and persists via ac_config_sync.
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "settings.h"

#include "additions/toggles/ac_toggles.h"
#include "additions/overlay/ac_hud.h"
#include "additions/overlay/ac_menu_tabs.h"
#include "additions/overlay/ac_overlay.h"
#include "additions/overlay/ac_overlay_rpc.h"
#include "additions/overlay/ac_reinfo.h"
#include "hooks/sdl/gamepad_trampoline.h"
#include "hooks/ui/font_trampoline.h"

extern "C" const char *ac_vk_gpu_name(void);
extern "C" void ac_vk_extent(int *w, int *h);
extern "C" void ac_vk_recreate_swapchain(void);
extern "C" volatile int g_debug_frame_request;
extern "C" bool g_gpuprof_frame;
extern "C" int extra_get_guest_voice_language(void);

namespace {

constexpr int kDefaultTargetFps = 60;
constexpr int kSliderMaxFps = 240;
constexpr int kHardMaxFps = 1000;
constexpr int kResPresetCount = 5;
constexpr float kMenuDefaultWidth = 540.0f;
constexpr float kMenuDefaultHeight = 580.0f;
constexpr float kMenuDefaultPosX = 36.0f;
constexpr float kMenuDefaultPosY = 36.0f;
constexpr int kMenuTabCount = 5;

const char *const kPresentModes[] = {"mailbox", "fifo", "fifo_relaxed", "immediate"};

struct ResPreset {
    int w;
    int h;
    const char *label;
};

const ResPreset kResPresets[kResPresetCount] = {
    {1280, 720, "1280x720 (HD)"},
    {1600, 900, "1600x900 (HD+ Padrao)"},
    {1920, 1080, "1920x1080 (Full HD)"},
    {2560, 1440, "2560x1440 (QHD)"},
    {3840, 2160, "3840x2160 (4K UHD)"}
};

bool env_on(const char *a, const char *b) {
    const char *e = getenv(a);
    if (!e && b) {
        e = getenv(b);
    }
    return e && atoi(e) > 0;
}

int eff_fps_limit(void) {
    if (g_extra_fps_override >= 0) {
        return std::clamp(g_extra_fps_override, 0, kHardMaxFps);
    }
    const char *e = getenv("AC_FPS");
    if (!e || !*e) {
        e = getenv("AC_TARGET_FPS");
    }
    return (e && *e) ? std::clamp(atoi(e), 0, kHardMaxFps) : kDefaultTargetFps;
}

bool eff_guard(void) {
    return g_extra_vtx_guard >= 0 ? g_extra_vtx_guard != 0 : env_on("LARECOMP_GUARD_VTX", "AC_GUARD_VTX");
}

bool eff_debug(void) {
    return g_extra_vtx_debug >= 0 ? g_extra_vtx_debug != 0 : env_on("LARECOMP_DEBUG_VTX", "AC_DEBUG_VTX");
}

float eff_lod_bias(void) {
    if (g_extra_lod_bias >= 0.0f) {
        return g_extra_lod_bias;
    }
    const char *e = getenv("LARECOMP_LOD_BIAS");
    return e ? static_cast<float>(atof(e)) : 0.0f;
}

float eff_lod_min(void) {
    if (g_extra_lod_min >= 0.0f) {
        return g_extra_lod_min;
    }
    const char *e = getenv("LARECOMP_LOD_MIN");
    return e ? static_cast<float>(atof(e)) : 0.0f;
}

bool eff_no_shadow(void) {
    if (g_extra_no_shadow >= 0) {
        return g_extra_no_shadow != 0;
    }
    return getenv("LARECOMP_NO_SHADOW") != nullptr;
}

int present_index(void) {
    const char *m = g_extra_present_mode;
    if (!m) {
        m = getenv("LARECOMP_VK_PRESENT");
    }
    if (!m) {
        m = "mailbox";
    }
    for (int i = 0; i < 4; i++) {
        if (!strcmp(m, kPresentModes[i])) {
            return i;
        }
    }
    return !strcmp(m, "fifo-relaxed") ? 2 : 0;
}

void render_top_banner(void) {
    int out_w = 1600;
    int out_h = 900;
    ac_vk_extent(&out_w, &out_h);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, AcIrisColors::BgCard);
    if (ImGui::BeginChild("##iris_banner", ImVec2(0, 54.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar)) {
        ImVec2 bp = ImGui::GetWindowPos();
        ImVec2 bs = ImGui::GetWindowSize();
        ac_ui_draw_iris_corners(bp, ImVec2(bp.x + bs.x, bp.y + bs.y), IM_COL32(0, 212, 255, 200), 6.0f, 1.5f);
        ImGui::TextColored(AcIrisColors::Cyan, "NEURON // LEGATUS SYSTEM TERMINAL");
        ImGui::SameLine(bs.x - 190.0f);
        ImGui::TextColored(AcIrisColors::CyanBright, "[INSERT / L3+R3 2s] FECHAR");
        ImGui::TextColored(AcIrisColors::TextMuted, "GPU: %s  ·  Saída: %dx%d", ac_vk_gpu_name(), out_w, out_h);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void tab_performance(void) {
    ac_perf_hud_dashboard_section(kPresentModes[present_index()]);

    ac_ui_section_header("SINCRONIA & CADÊNCIA DE QUADROS", "// PACING");
    int cur_fps = eff_fps_limit();
    if (ImGui::SliderInt("Limite de FPS", &cur_fps, 0, kSliderMaxFps,
                         cur_fps == 0 ? "Ilimitado (0)" : "%d FPS")) {
        g_extra_fps_override = std::clamp(cur_fps, 0, kHardMaxFps);
        ac_config_sync_save_hook();
    }

    static const int kQuickFps[] = {0, 30, 60, 120, 144};
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            ImGui::SameLine();
        }
        char label[24];
        snprintf(label, sizeof(label), kQuickFps[i] == 0 ? "Ilimitado" : "%d", kQuickFps[i]);
        if (ImGui::SmallButton(label)) {
            g_extra_fps_override = kQuickFps[i];
            ac_config_sync_save_hook();
        }
    }

    int pm = present_index();
    if (ImGui::Combo("Modo VSync (Swapchain)", &pm,
                     "Mailbox (Baixa Latência)\0FIFO (VSync Estrito)\0FIFO Relaxed (Adaptativo)\0Immediate (Sem Limite)\0")) {
        g_extra_present_mode = kPresentModes[pm];
        g_extra_present_pending = 1;
        ac_config_sync_save_hook();
    }
}

void tab_graphics(void) {
    ac_ui_section_header("JANELA & EXIBIÇÃO", "// SYS.DISP");
    int cur_w = g_extra_win_w > 0 ? g_extra_win_w : 1600;
    int cur_h = g_extra_win_h > 0 ? g_extra_win_h : 900;
    int res_idx = kResPresetCount;
    for (int i = 0; i < kResPresetCount; i++) {
        if (kResPresets[i].w == cur_w && kResPresets[i].h == cur_h) {
            res_idx = i;
            break;
        }
    }
    char custom_buf[48];
    snprintf(custom_buf, sizeof(custom_buf), "Personalizada (%dx%d)", cur_w, cur_h);
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
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    bool fs = (g_extra_fullscreen_pending >= 0 ? g_extra_fullscreen_pending : g_sdl_fullscreen) != 0;
    if (ImGui::Checkbox("Tela Cheia (F11)", &fs)) {
        g_extra_fullscreen_pending = fs ? 1 : 0;
        ac_config_sync_save_hook();
    }

    ac_ui_section_header("RECONSTRUÇÃO & SUPER RESOLUÇÃO (AMD FSR)", "// SYS.FSR");
    int up = g_settings.upscaler == UPSCALE_FSR3 ? 2 : (g_settings.upscaler == UPSCALE_FSR1 ? 1 : 0);
    if (ImGui::Combo("Upscaler", &up,
                     "Off (Bilinear)\0AMD FSR 1.0 (Espacial)\0AMD FSR 3.1 (Temporal + Jitter + RCAS)\0")) {
        g_settings.upscaler = (up == 2) ? UPSCALE_FSR3 : (up == 1 ? UPSCALE_FSR1 : UPSCALE_BILINEAR);
        settings_save();
    }

    static const float s_scales[] = {0.50f, 0.58f, 0.67f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f};
    float cur_sc = g_extra_render_scale > 0.0f ? g_extra_render_scale : (g_settings.render_scale > 0.0f ? g_settings.render_scale : 1.0f);
    int sc_idx = 4;
    for (int i = 0; i < 9; i++) {
        if (cur_sc >= s_scales[i] - 0.03f && cur_sc <= s_scales[i] + 0.03f) {
            sc_idx = i;
        }
    }
    const char *scale_items =
        (up == 2) ? "0.50x (FSR 3.1 Performance)\00.58x (FSR 3.1 Balanced)\00.67x (FSR 3.1 Quality)\00.75x (FSR 3.1 Ultra Quality)\0"
                    "1.00x (FSR 3.1 Native AA)\01.25x (FSR 3.1 SSAA Qualidade)\01.50x (FSR 3.1 SSAA Ultra)\0"
                    "1.75x (FSR 3.1 SSAA Extremo)\02.00x (FSR 3.1 SSAA 4K)\0"
      : (up == 1) ? "0.50x (FSR 1.0 Performance)\00.58x (FSR 1.0 Balanced)\00.67x (FSR 1.0 Quality)\00.75x (FSR 1.0 Ultra Quality)\0"
                    "1.00x (FSR 1.0 Nativo)\01.25x (FSR 1.0 SSAA Qualidade)\01.50x (FSR 1.0 SSAA Ultra)\0"
                    "1.75x (FSR 1.0 SSAA Extremo)\02.00x (FSR 1.0 SSAA 4K)\0"
                  : "0.50x (Performance)\00.58x (Balanced)\00.67x (Quality)\00.75x (Ultra Quality)\0"
                    "1.00x (Nativo 1:1)\01.25x (SSAA Qualidade)\01.50x (SSAA Ultra)\01.75x (SSAA Extremo)\02.00x (SSAA 4K)\0";
    if (ImGui::Combo("Escala Interna / SSAA", &sc_idx, scale_items)) {
        cur_sc = s_scales[sc_idx];
        g_extra_render_scale = g_settings.render_scale = cur_sc;
        settings_save();
    }

    bool fg = (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) != 0;
    if (ImGui::Checkbox("FSR 3.1 Frame Generation (Interpolação 2x FPS)", &fg)) {
        g_extra_framegen = g_settings.framegen = fg ? 1 : 0;
        settings_save();
    }
    bool jit = (g_extra_fsr_jitter >= 0 ? g_extra_fsr_jitter : g_settings.fsr_jitter) != 0;
    if (ImGui::Checkbox("Camera Jitter Sub-Pixel (Halton 2,3)", &jit)) {
        g_extra_fsr_jitter = g_settings.fsr_jitter = jit ? 1 : 0;
        settings_save();
    }
    float shp = g_extra_fsr_sharpness >= 0.0f ? g_extra_fsr_sharpness : g_settings.fsr_sharpness;
    if (ImGui::SliderFloat("Nitidez FSR (RCAS)", &shp, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_fsr_sharpness = g_settings.fsr_sharpness = shp;
        settings_save();
    }

    ac_ui_section_header("QUALIDADE & FILTROS", "// SYS.GFX");
    int aa = g_settings.antialias;
    if (ImGui::Combo("Antialiasing", &aa, "Desativado\0SMAA Ultra (Color Edge)\0")) {
        g_settings.antialias = aa;
        settings_save();
    }

    float bias = eff_lod_bias();
    if (ImGui::SliderFloat("Texturas (Bias LOD)", &bias, 0.0f, 3.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_bias = bias;
        ac_config_sync_save_hook();
    }
    float lod = eff_lod_min();
    if (ImGui::SliderFloat("LoD Mínimo", &lod, 0.0f, 4.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_min = lod;
        ac_config_sync_save_hook();
    }
    bool shadow = !eff_no_shadow();
    if (ImGui::Checkbox("Sombras Dinâmicas", &shadow)) {
        g_extra_no_shadow = shadow ? 0 : 1;
        ac_config_sync_save_hook();
    }
    ImGui::TextDisabled("Nota: Bias LOD, LoD Mínimo e Sombras aplicam-se a novos samplers (reinicie para cache completo).");
}

void tab_game_and_rpc(void) {
    ac_ui_section_header("IDIOMA & COMPORTAMENTO", "// SYS.LOC");
    static const char *const lang_items =
        "Auto (Sistema)\0Japonês (ja)\0Inglês (en-US)\0Espanhol (es)\0Francês (fr)\0"
        "Alemão (de)\0Italiano (it)\0Holandês (nl)\0Russo (ru)\0Coreano (ko)\0"
        "Chinês Simplificado (zh-CN)\0Chinês Tradicional (zh-TW)\0Português (Brasil) (pt-BR)\0";

    int cur_lang = (g_settings.language >= 0 && g_settings.language < N_LANGUAGES) ? g_settings.language : 0;
    if (ImGui::Combo("Idioma dos Textos (Reinício)", &cur_lang, lang_items)) {
        g_settings.language = cur_lang;
        settings_save();
    }
    ImGui::TextDisabled("Ativo: %s (%s) — requer reiniciar o jogo para trocar textos.",
                        settings_language(), settings_language_name());

    static const char *const voice_items = "Auto (Acompanha Idioma)\0Japonês (ja)\0Inglês (en-US)\0";
    int cur_voice = (g_settings.voice_language >= 0 && g_settings.voice_language <= 2) ? g_settings.voice_language : 0;
    if (ImGui::Combo("Idioma das Vozes", &cur_voice, voice_items)) {
        g_settings.voice_language = cur_voice;
        settings_save();
    }
    int g_v = extra_get_guest_voice_language();
    ImGui::TextDisabled("Dublagem ativa no motor: %s",
                        g_v == 0 ? "Japonês (ja)" : g_v == 1 ? "Inglês (en-US)" : "Aguardando boot");

    bool ignore_focus = (g_extra_ignore_focus >= 0) ? (g_extra_ignore_focus != 0) : (getenv("AC_IGNORE_FOCUS") != nullptr);
    if (ImGui::Checkbox("Rodar em Segundo Plano (Ignorar Perda de Foco)", &ignore_focus)) {
        g_extra_ignore_focus = ignore_focus ? 1 : 0;
        ac_config_sync_save_hook();
    }
    const char *cs_env = getenv("AC_CUTSCENE_FPS");
    bool cs_30 = (g_extra_cutscene_30fps >= 0) ? (g_extra_cutscene_30fps != 0) : (cs_env && atoi(cs_env) == 30);
    if (ImGui::Checkbox("Travar Cutscenes em 30 FPS (Pacing Original)", &cs_30)) {
        g_extra_cutscene_30fps = cs_30 ? 1 : 0;
        ac_config_sync_save_hook();
    }

    ac_ui_section_header("CONTROLES & ÍCONES DE BOTÕES", "// SYS.INPUT");
    static const char *const prompt_items =
        "Automático (Xbox / Switch / Teclado)\0Controle Xbox (Padrão PC)\0Nintendo Switch (Original)\0Teclado & Mouse (PC)\0";
    static const int kPromptModes[] = {
        EXTRA_PROMPT_MODE_AUTO,
        EXTRA_PROMPT_MODE_XBOX,
        EXTRA_PROMPT_MODE_SWITCH,
        EXTRA_PROMPT_MODE_KBM
    };
    int prompt_idx = 0;
    if (g_extra_prompts_override == EXTRA_PROMPT_MODE_XBOX) {
        prompt_idx = 1;
    } else if (g_extra_prompts_override == EXTRA_PROMPT_MODE_SWITCH) {
        prompt_idx = 2;
    } else if (g_extra_prompts_override == EXTRA_PROMPT_MODE_KBM) {
        prompt_idx = 3;
    }
    if (ImGui::Combo("Prompts de Botões (HUD)", &prompt_idx, prompt_items)) {
        extra_set_prompts_override(kPromptModes[prompt_idx]);
        ac_config_sync_save_hook();
    }
    bool confirm_a = extra_gamepad_confirm_a_enabled() != 0;
    if (ImGui::Checkbox("Confirmar com Botão A (Inverter B -> A no Controle)", &confirm_a)) {
        extra_set_gamepad_confirm_a(confirm_a ? 1 : 0);
        ac_config_sync_save_hook();
    }

    AcOverlayRpc::RenderSection();
}

void tab_diagnostics(void) {
    ac_ui_section_header("ESTABILIDADE & SUBMISSÃO (NVN)", "// SYS.DBG");
    bool guard = eff_guard();
    if (ImGui::Checkbox("Vertex Guard (Clamp de Explosão de Vértices)", &guard)) {
        g_extra_vtx_guard = guard ? 1 : 0;
    }
    bool dbg = eff_debug();
    if (ImGui::Checkbox("Vertex Debug Logging (NaN/Inf em stderr)", &dbg)) {
        g_extra_vtx_debug = dbg ? 1 : 0;
    }

    bool async_env = env_on("AC_ASYNC_SUBMIT", nullptr);
    bool async_on = async_env && !g_extra_async_kill;
    ImGui::BeginDisabled(!async_env);
    if (ImGui::Checkbox("Async Submit Worker (Desligamento instantâneo / One-way)", &async_on) && !async_on) {
        extra_async_submit_kill();
        ac_config_sync_save_hook();
    }
    ImGui::EndDisabled();

    ac_ui_section_header("ENGENHARIA REVERSA & CAPTURA", "// SYS.RE");
    bool reinfo = ac_reinfo_visible() != 0;
    if (ImGui::Checkbox("Janela RE Info — Memória do Jogo ao Vivo (F8)", &reinfo)) {
        ac_reinfo_toggle();
    }
    ImGui::Checkbox("GPU Profiler por Render Pass (g_gpuprof_frame)", &g_gpuprof_frame);
    if (ImGui::Button("Capturar Dump de Frame de Depuração (F9)", ImVec2(-1.0f, 0.0f))) {
        g_debug_frame_request = 1;
    }

    ac_ui_section_header("ATALHOS DO TERMINAL", "// KEYS");
    ImGui::BulletText("Insert / L3+R3 (2s): Terminal | F8: RE Info | F9: Frame Dump");
    ImGui::BulletText("LB / RB (Controle): Abas | F10: Mouse Capture | F11: Tela Cheia");
}

} // namespace

static int s_active_tab = 0;

void ac_menu_apply_pending(void) {
    if (!g_extra_present_pending) {
        return;
    }
    g_extra_present_pending = 0;
    printf("[overlay] present mode -> %s (swapchain recreated)\n", kPresentModes[present_index()]);
    fflush(stdout);
    ac_vk_recreate_swapchain();
}

void ac_menu_frame(void) {
    ImGui::SetNextWindowSize(ImVec2(kMenuDefaultWidth, kMenuDefaultHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(kMenuDefaultPosX, kMenuDefaultPosY), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ASTRAL CHAIN // IRIS TERMINAL###ac_main_menu", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ac_ui_draw_window_chrome();
        render_top_banner();
        ImGui::Spacing();
        int force_tab = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) {
            force_tab = (s_active_tab + kMenuTabCount - 1) % kMenuTabCount;
        } else if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) {
            force_tab = (s_active_tab + 1) % kMenuTabCount;
        }
        if (ImGui::BeginTabBar("##iris_tabs")) {
            static const char *const kTabLabels[kMenuTabCount] = {
                " Desempenho ",
                " Gráficos ",
                " Áudio & Mouse ",
                " Jogo & RPC ",
                " Diagnóstico "
            };
            static void (*const kTabFns[kMenuTabCount])(void) = {
                tab_performance,
                tab_graphics,
                ac_menu_tab_audio_mouse,
                tab_game_and_rpc,
                tab_diagnostics
            };
            for (int i = 0; i < kMenuTabCount; i++) {
                ImGuiTabItemFlags flags = (force_tab == i) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(kTabLabels[i], nullptr, flags)) {
                    s_active_tab = i;
                    kTabFns[i]();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
