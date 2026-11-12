// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] In-game Legatus / IRIS dashboard (Insert): Performance / Graphics / Game & RPC / Diagnostics.
// Writes directly onto runtime toggles (extra/toggles/ac_toggles.h) or g_settings.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "settings.h"

#include "additions/toggles/ac_toggles.h"
#include "additions/overlay/ac_hud.h"
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
extern "C" void extra_set_guest_voice_language(int voice_lang);

static const char *s_present_modes[] = {"mailbox", "fifo", "fifo_relaxed", "immediate"};
static const int s_fps_steps[] = {0, 30, 60, 90, 120, 144, 165};
static bool s_recreate_pending;
static const char *s_present_pending;

void ac_menu_apply_pending(void) {
    if (!s_recreate_pending) return;
    s_recreate_pending = false;
    g_extra_present_mode = s_present_pending;
    printf("[overlay] present mode -> %s (swapchain recreated)\n", s_present_pending);
    fflush(stdout);
    ac_vk_recreate_swapchain();
}

static bool env_on(const char *a, const char *b) {
    const char *e = getenv(a);
    if (!e && b) e = getenv(b);
    return e && atoi(e) > 0;
}

static bool eff_guard(void) {
    return g_extra_vtx_guard >= 0 ? g_extra_vtx_guard != 0 : env_on("LARECOMP_GUARD_VTX", "AC_GUARD_VTX");
}

static bool eff_debug(void) {
    return g_extra_vtx_debug >= 0 ? g_extra_vtx_debug != 0 : env_on("LARECOMP_DEBUG_VTX", "AC_DEBUG_VTX");
}

static float eff_lod_bias(void) {
    if (g_extra_lod_bias >= 0.0f) return g_extra_lod_bias;
    const char *e = getenv("LARECOMP_LOD_BIAS");
    return e ? (float)atof(e) : 0.0f;
}

static float eff_lod_min(void) {
    if (g_extra_lod_min >= 0.0f) return g_extra_lod_min;
    const char *e = getenv("LARECOMP_LOD_MIN");
    return e ? (float)atof(e) : 0.0f;
}

static bool eff_no_shadow(void) {
    if (g_extra_no_shadow >= 0) return g_extra_no_shadow != 0;
    return getenv("LARECOMP_NO_SHADOW") != NULL;
}

static int present_index(void) {
    const char *m = g_extra_present_mode;
    if (!m) m = getenv("LARECOMP_VK_PRESENT");
    if (!m) m = "mailbox";
    for (int i = 0; i < 4; i++) if (!strcmp(m, s_present_modes[i])) return i;
    return !strcmp(m, "fifo-relaxed") ? 2 : 0;
}

static void render_top_banner(void) {
    int out_w = 1600, out_h = 900;
    ac_vk_extent(&out_w, &out_h);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, AcIrisColors::BgCard);
    if (ImGui::BeginChild("##iris_banner", ImVec2(0, 54.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar)) {
        ImVec2 bp = ImGui::GetWindowPos(), bs = ImGui::GetWindowSize();
        ac_ui_draw_iris_corners(bp, ImVec2(bp.x + bs.x, bp.y + bs.y), IM_COL32(0, 212, 255, 200), 6.0f, 1.5f);
        ImGui::TextColored(AcIrisColors::Cyan, "NEURON // LEGATUS SYSTEM TERMINAL");
        ImGui::SameLine(bs.x - 190.0f);
        ImGui::TextColored(AcIrisColors::CyanBright, "[INSERT / L3+R3 2s] FECHAR");
        ImGui::TextColored(AcIrisColors::TextMuted, "GPU: %s  ·  Saída: %dx%d", ac_vk_gpu_name(), out_w, out_h);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void tab_performance(void) {
    ac_perf_hud_dashboard_section(s_present_modes[present_index()]);

    ac_ui_section_header("SINCRONIA & CADÊNCIA DE QUADROS", "// PACING");
    int sel = 0;
    for (int i = 1; i < 7; i++) if (s_fps_steps[i] == g_extra_fps_override) sel = i;
    if (ImGui::Combo("Limite de FPS", &sel, "Padrão do Ambiente (AC_FPS)\0 30 FPS\0 60 FPS\0 90 FPS\0 120 FPS\0 144 FPS\0 165 FPS\0")) {
        g_extra_fps_override = s_fps_steps[sel];
        printf("[overlay] fps limit -> %d%s\n", s_fps_steps[sel], sel ? "" : " (env)");
    }
    int pm = present_index();
    if (ImGui::Combo("Modo VSync (Swapchain)", &pm,
                     "Mailbox (Baixa Latência)\0FIFO (VSync Estrito)\0FIFO Relaxed (Adaptativo)\0Immediate (Sem Limite)\0")) {
        s_present_pending = s_present_modes[pm];
        s_recreate_pending = true;
    }
}

static void tab_graphics(void) {
    ac_ui_section_header("RECONSTRUÇÃO & SUPER RESOLUÇÃO (AMD FSR)", "// SYS.FSR");
    int up = g_settings.upscaler == UPSCALE_FSR3 ? 2 : (g_settings.upscaler == UPSCALE_FSR1 ? 1 : 0);
    if (ImGui::Combo("Upscaler", &up,
                     "Off (Bilinear)\0AMD FSR 1.0 (Espacial)\0AMD FSR 3.1 (Temporal + Jitter + RCAS)\0")) {
        g_settings.upscaler = (up == 2) ? UPSCALE_FSR3 : (up == 1 ? UPSCALE_FSR1 : UPSCALE_BILINEAR);
        settings_save();
        printf("[overlay] upscaler -> %s\n", up == 2 ? "fsr3.1" : (up == 1 ? "fsr1" : "bilinear"));
    }

    static const float s_scales[] = {0.50f, 0.58f, 0.67f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f};
    float cur_sc = g_extra_render_scale > 0.0f ? g_extra_render_scale : (g_settings.render_scale > 0.0f ? g_settings.render_scale : 1.0f);
    int sc_idx = 4;
    for (int i = 0; i < 9; i++) {
        if (cur_sc >= s_scales[i] - 0.03f && cur_sc <= s_scales[i] + 0.03f) {
            sc_idx = i;
        }
    }
    const char *fsr_scale_items =
        (up == 2) ? "0.50x (FSR 3.1 Performance)\00.58x (FSR 3.1 Balanced)\00.67x (FSR 3.1 Quality)\00.75x (FSR 3.1 Ultra Quality)\0"
                    "1.00x (FSR 3.1 Native AA)\01.25x (FSR 3.1 Supersampling)\01.50x (FSR 3.1 Supersampling)\0"
                    "1.75x (FSR 3.1 Supersampling)\02.00x (FSR 3.1 Supersampling 4K)\0"
      : (up == 1) ? "0.50x (FSR 1.0 Performance)\00.58x (FSR 1.0 Balanced)\00.67x (FSR 1.0 Quality)\00.75x (FSR 1.0 Ultra Quality)\0"
                    "1.00x (FSR 1.0 Nativo EASU+RCAS)\01.25x (FSR 1.0 Supersampling)\01.50x (FSR 1.0 Supersampling)\0"
                    "1.75x (FSR 1.0 Supersampling)\02.00x (FSR 1.0 Supersampling 4K)\0"
                  : "0.50x (Performance)\00.58x (Balanced)\00.67x (Quality)\00.75x (Ultra Quality)\0"
                    "1.00x (Nativo 1:1)\01.25x (SSAA Qualidade)\01.50x (SSAA Ultra)\01.75x (SSAA Extremo)\02.00x (SSAA 4K)\0";
    if (ImGui::Combo("Escala / Supersampling FSR", &sc_idx, fsr_scale_items)) {
        cur_sc = s_scales[sc_idx];
        g_extra_render_scale = g_settings.render_scale = cur_sc;
        settings_save();
        printf("[overlay] render scale -> %.2fx\n", cur_sc);
    }

    bool fg = (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) != 0;
    if (ImGui::Checkbox("FSR 3.1 Frame Generation (Interpolação 2x FPS)", &fg)) {
        g_extra_framegen = g_settings.framegen = fg ? 1 : 0;
        settings_save();
        printf("[overlay] fsr3 framegen -> %s\n", fg ? "on" : "off");
    }
    bool jit = (g_extra_fsr_jitter >= 0 ? g_extra_fsr_jitter : g_settings.fsr_jitter) != 0;
    if (ImGui::Checkbox("Camera Jitter Sub-Pixel (Halton 2,3)", &jit)) {
        g_extra_fsr_jitter = g_settings.fsr_jitter = jit ? 1 : 0;
        settings_save();
        printf("[overlay] fsr3 jitter -> %s\n", jit ? "on" : "off");
    }
    float shp = g_extra_fsr_sharpness >= 0.0f ? g_extra_fsr_sharpness : g_settings.fsr_sharpness;
    if (ImGui::SliderFloat("Nitidez FSR 1.0 / 3.1 (RCAS)", &shp, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_fsr_sharpness = g_settings.fsr_sharpness = shp;
        settings_save();
    }

    ac_ui_section_header("QUALIDADE DE RENDERIZAÇÃO & FILTROS", "// SYS.GFX");
    int aa = g_settings.antialias;
    if (ImGui::Combo("Antialiasing", &aa, "Desativado\0SMAA Ultra (Color Edge)\0")) {
        g_settings.antialias = aa;
        settings_save();
        printf("[overlay] antialias -> %s\n", aa == AA_SMAA ? "smaa" : "off");
    }
    static const float s_ssaa[] = {1.00f, 1.25f, 1.50f, 1.75f, 2.00f};
    int ss_idx = 0;
    for (int i = 0; i < 5; i++) {
        if (cur_sc >= s_ssaa[i] - 0.05f) {
            ss_idx = i;
        }
    }
    const char *ss_items =
        (up == 2) ? "Desativado (1.00x FSR 3.1 Native AA)\01.25x FSR 3.1 Supersampling (Qualidade)\0"
                    "1.50x FSR 3.1 Supersampling (Ultra)\01.75x FSR 3.1 Supersampling (Extremo)\02.00x FSR 3.1 Supersampling (4K)\0"
      : (up == 1) ? "Desativado (1.00x FSR 1.0 Nativo)\01.25x FSR 1.0 Supersampling (Qualidade)\0"
                    "1.50x FSR 1.0 Supersampling (Ultra)\01.75x FSR 1.0 Supersampling (Extremo)\02.00x FSR 1.0 Supersampling (4K)\0"
                  : "Desativado (1.00x Nativo)\01.25x SSAA (Qualidade)\01.50x SSAA (Ultra)\0"
                    "1.75x SSAA (Extremo)\02.00x SSAA (4K Downsampling)\0";
    if (ImGui::Combo("Supersampling (SSAA)", &ss_idx, ss_items)) {
        cur_sc = s_ssaa[ss_idx];
        g_extra_render_scale = g_settings.render_scale = cur_sc;
        settings_save();
        printf("[overlay] ssaa scale -> %.2fx\n", cur_sc);
    }
    int out_w = 1600, out_h = 900;
    ac_vk_extent(&out_w, &out_h);
    double ss_mult = cur_sc > 1.0f ? (double)cur_sc : 1.0;
    const char *up_tag = (up == 2) ? "FSR 3.1 SSAA" : (up == 1 ? "FSR 1.0 SSAA" : "SSAA");
    ImGui::TextDisabled("Pipeline ativo: 1600x900 -> %dx%d (%s %.2fx) -> %dx%d | Anisotrópico 16x",
                        (int)(out_w * ss_mult), (int)(out_h * ss_mult), up_tag, cur_sc, out_w, out_h);
    float bias = eff_lod_bias();
    if (ImGui::SliderFloat("Texturas (Bias LOD)", &bias, 0.0f, 3.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_bias = bias;
    }
    float lod = eff_lod_min();
    if (ImGui::SliderFloat("LoD Mínimo", &lod, 0.0f, 4.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_min = lod;
    }
    bool shadow = !eff_no_shadow();
    if (ImGui::Checkbox("Sombras Dinâmicas", &shadow)) {
        g_extra_no_shadow = shadow ? 0 : 1;
    }
}

static void tab_game_and_rpc(void) {
    ac_ui_section_header("IDIOMA & DUBLAGEM", "// SYS.LOC");
    static const char *const lang_items =
        "Auto (Sistema)\0Japonês (ja)\0Inglês (en-US)\0Espanhol (es)\0Francês (fr)\0"
        "Alemão (de)\0Italiano (it)\0Holandês (nl)\0Russo (ru)\0Coreano (ko)\0"
        "Chinês Simplificado (zh-CN)\0Chinês Tradicional (zh-TW)\0Português (Brasil) (pt-BR)\0";

    int cur_lang = (g_settings.language >= 0 && g_settings.language < N_LANGUAGES) ? g_settings.language : 0;
    if (ImGui::Combo("Idioma dos Textos", &cur_lang, lang_items)) {
        g_settings.language = cur_lang;
        settings_save();
    }
    ImGui::TextDisabled("Ativo: %s (%s) — reinicie o jogo para trocar textos.",
                        settings_language(), settings_language_name());

    ImGui::Spacing();
    static const char *const voice_items =
        "Auto (Acompanha Idioma)\0Japonês (ja)\0Inglês (en-US)\0";
    int cur_voice = (g_settings.voice_language >= 0 && g_settings.voice_language <= 2) ? g_settings.voice_language : 0;
    if (ImGui::Combo("Idioma das Vozes", &cur_voice, voice_items)) {
        g_settings.voice_language = cur_voice;
        settings_save();
        extra_set_guest_voice_language(cur_voice == 1 ? 0 : (cur_voice == 2 ? 1 : -1));
    }
    int g_v = extra_get_guest_voice_language();
    ImGui::TextDisabled("Dublagem ativa no motor: %s",
                        g_v == 0 ? "Japonês (ja)" : g_v == 1 ? "Inglês (en-US)" : "Aguardando boot");

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
    }
    bool confirm_a = extra_gamepad_confirm_a_enabled() != 0;
    if (ImGui::Checkbox("Confirmar / Ação com Botão A (Inverter B -> A no Controle)", &confirm_a)) {
        extra_set_gamepad_confirm_a(confirm_a ? 1 : 0);
    }
    int active_pm = extra_prompts_active();
    const char *pm_label = active_pm == EXTRA_PROMPT_MODE_XBOX ? "Controle Xbox" :
                           (active_pm == EXTRA_PROMPT_MODE_SWITCH ? "Nintendo Switch" : "Teclado & Mouse");
    ImGui::TextDisabled("Layout ativo na tela: %s | Confirmar: %s",
                        pm_label,
                        confirm_a ? "Botão A (Padrão PC/Xbox)" : "Botão B (Posição Switch)");

    AcOverlayRpc::RenderSection();
}

static void tab_diagnostics(void) {
    ac_ui_section_header("ESTABILIDADE & SUBMISSÃO (NVN)", "// SYS.DBG");
    bool guard = eff_guard();
    if (ImGui::Checkbox("Vertex Guard (Clamp de Explosão de Vértices)", &guard)) g_extra_vtx_guard = guard ? 1 : 0;
    bool dbg = eff_debug();
    if (ImGui::Checkbox("Vertex Debug Logging (NaN/Inf em stderr)", &dbg)) g_extra_vtx_debug = dbg ? 1 : 0;

    bool async_env = env_on("AC_ASYNC_SUBMIT", nullptr);
    bool async_on = async_env && !g_extra_async_kill;
    ImGui::BeginDisabled(!async_env);
    if (ImGui::Checkbox("Async Submit Worker (Submissão Paralela)", &async_on) && !async_on) {
        extra_async_submit_kill();
    }
    ImGui::EndDisabled();

    ac_ui_section_header("ENGENHARIA REVERSA & CAPTURA", "// SYS.RE");
    bool reinfo = ac_reinfo_visible() != 0;
    if (ImGui::Checkbox("Janela RE Info — Memória do Jogo ao Vivo (F8)", &reinfo)) ac_reinfo_toggle();
    ImGui::Checkbox("GPU Profiler por Render Pass (g_gpuprof_frame)", &g_gpuprof_frame);
    if (ImGui::Button("Capturar Dump de Frame de Depuração (F9)", ImVec2(-1.0f, 0.0f))) {
        g_debug_frame_request = 1;
    }

    ac_ui_section_header("COMANDOS RÁPIDOS DO TERMINAL", "// KEYS");
    ImGui::BulletText("Insert / L3+R3 (2s) : Abre / fecha este terminal IRIS");
    ImGui::BulletText("LB / RB (Controle)  : Alterna entre as abas do menu");
    ImGui::BulletText("F8                  : Abre / fecha o inspetor RE Info (memória guest)");
    ImGui::BulletText("F9                  : Dump de passes do frame atual");
    ImGui::BulletText("F10                 : Alterna captura do ponteiro do mouse");
    ImGui::BulletText("F11                 : Alterna modo Tela Cheia | Alt+F4 : Encerrar");
}

constexpr float kMenuDefaultWidth = 530.0f;
constexpr float kMenuDefaultHeight = 560.0f;
constexpr float kMenuDefaultPosX = 36.0f;
constexpr float kMenuDefaultPosY = 36.0f;
constexpr int kMenuTabCount = 4;

static int s_active_tab = 0;

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
                "  Desempenho  ",
                "  Gráficos  ",
                "  Jogo & RPC  ",
                "  Diagnóstico  "
            };
            static void (*const kTabFns[kMenuTabCount])(void) = {
                tab_performance,
                tab_graphics,
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
