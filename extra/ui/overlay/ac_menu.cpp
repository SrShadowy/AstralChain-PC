// [extra] In-game menu content (Insert): Performance / Graphics / Debug / About.
// Everything writes straight onto the runtime toggles (extra/toggles/ac_toggles.h)
// or g_settings, both read per-frame downstream; env vars stay the boot default.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "settings.h"  // (runtime/las: g_settings, AA_*, UPSCALE_*)

#include "extra/toggles/ac_toggles.h"
#include "extra/ui/overlay/ac_overlay.h"
#include "extra/ui/overlay/ac_overlay_rpc.h"
#include "extra/ui/overlay/ac_reinfo.h"

extern "C" const char *ac_vk_gpu_name(void);   // (ac_vk_glue.cpp)
extern "C" void ac_vk_recreate_swapchain(void);
extern "C" volatile int g_debug_frame_request; // (game_hooks_neutral.c; also F9)
extern "C" bool g_gpuprof_frame;               // (nvn/vulkan: per-frame GPU timings)

// GNOME Adwaita (libadwaita) dark palette, with the project teal (#3DD1B8) kept as the
// interactive accent. Neutral grays follow GTK4 dark: #242424 / #303030 / #3c3c3c / #474747.
void ac_menu_apply_style(void) {
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 8.0f;
    style.TabRounding       = 8.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize  = 1.0f;
    style.PopupBorderSize  = 1.0f;

    style.WindowPadding     = ImVec2(12.0f, 12.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.CellPadding       = ImVec2(8.0f, 5.0f);

    // Adwaita dark neutrals.
    const ImVec4 bg_window    = ImVec4(0.141f, 0.141f, 0.141f, 1.00f); // #242424
    const ImVec4 bg_view      = ImVec4(0.118f, 0.118f, 0.118f, 1.00f); // #1e1e1e
    const ImVec4 bg_card      = ImVec4(0.188f, 0.188f, 0.188f, 1.00f); // #303030
    const ImVec4 bg_button    = ImVec4(0.180f, 0.180f, 0.180f, 1.00f); // #2e2e2e
    const ImVec4 bg_hover     = ImVec4(0.235f, 0.235f, 0.235f, 1.00f); // #3c3c3c
    const ImVec4 bg_active    = ImVec4(0.278f, 0.278f, 0.278f, 1.00f); // #474747
    const ImVec4 border       = ImVec4(0.235f, 0.235f, 0.235f, 0.50f); // #3c3c3c
    const ImVec4 text         = ImVec4(0.980f, 0.980f, 0.980f, 1.00f);
    const ImVec4 text_dim     = ImVec4(0.604f, 0.600f, 0.588f, 1.00f); // #9a9996
    // Project accent (teal) + Adwaita blue for data plots.
    const ImVec4 teal         = ImVec4(0.240f, 0.820f, 0.720f, 1.00f); // #3DD1B8
    const ImVec4 teal_hi      = ImVec4(0.340f, 0.920f, 0.820f, 1.00f);
    const ImVec4 blue         = ImVec4(0.208f, 0.518f, 0.894f, 1.00f); // #3584e4

    ImVec4 *c = style.Colors;
    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = text_dim;
    c[ImGuiCol_WindowBg]              = ImVec4(bg_window.x, bg_window.y, bg_window.z, 0.96f);
    c[ImGuiCol_ChildBg]               = ImVec4(bg_view.x, bg_view.y, bg_view.z, 0.60f);
    c[ImGuiCol_PopupBg]               = ImVec4(bg_card.x, bg_card.y, bg_card.z, 0.98f);
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]               = bg_button;
    c[ImGuiCol_FrameBgHovered]        = bg_hover;
    c[ImGuiCol_FrameBgActive]         = bg_active;
    c[ImGuiCol_TitleBg]               = bg_window;
    c[ImGuiCol_TitleBgActive]         = bg_card;
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(bg_window.x, bg_window.y, bg_window.z, 0.75f);
    c[ImGuiCol_MenuBarBg]             = bg_window;
    c[ImGuiCol_ScrollbarBg]           = ImVec4(bg_view.x, bg_view.y, bg_view.z, 0.60f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.360f, 0.360f, 0.360f, 1.00f); // #5c5c5c
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.420f, 0.420f, 0.420f, 1.00f); // #6b6b6b
    c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.478f, 0.478f, 0.478f, 1.00f); // #7a7a7a
    c[ImGuiCol_CheckMark]             = teal;
    c[ImGuiCol_SliderGrab]            = teal;
    c[ImGuiCol_SliderGrabActive]      = teal_hi;
    c[ImGuiCol_Button]                = bg_button;
    c[ImGuiCol_ButtonHovered]         = bg_hover;
    c[ImGuiCol_ButtonActive]          = bg_active;
    c[ImGuiCol_Header]                = ImVec4(bg_card.x, bg_card.y, bg_card.z, 0.80f);
    c[ImGuiCol_HeaderHovered]         = ImVec4(bg_hover.x, bg_hover.y, bg_hover.z, 0.90f);
    c[ImGuiCol_HeaderActive]          = bg_active;
    c[ImGuiCol_Separator]             = ImVec4(border.x, border.y, border.z, 0.60f);
    c[ImGuiCol_SeparatorHovered]      = ImVec4(border.x, border.y, border.z, 0.80f);
    c[ImGuiCol_SeparatorActive]       = teal;
    c[ImGuiCol_ResizeGrip]            = ImVec4(0.360f, 0.360f, 0.360f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]     = ImVec4(0.420f, 0.420f, 0.420f, 0.70f);
    c[ImGuiCol_ResizeGripActive]      = ImVec4(0.478f, 0.478f, 0.478f, 0.90f);
    c[ImGuiCol_Tab]                   = bg_window;
    c[ImGuiCol_TabHovered]            = bg_card;
    c[ImGuiCol_TabActive]             = bg_card;
    c[ImGuiCol_TabUnfocused]          = bg_view;
    c[ImGuiCol_TabUnfocusedActive]    = bg_card;
    c[ImGuiCol_TableHeaderBg]         = bg_button;
    c[ImGuiCol_TableBorderStrong]     = ImVec4(border.x, border.y, border.z, 0.75f);
    c[ImGuiCol_TableBorderLight]      = ImVec4(border.x, border.y, border.z, 0.45f);
    c[ImGuiCol_TableRowBg]            = ImVec4(bg_view.x, bg_view.y, bg_view.z, 0.55f);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(bg_card.x, bg_card.y, bg_card.z, 0.55f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(teal.x, teal.y, teal.z, 0.35f);
    c[ImGuiCol_NavHighlight]          = teal;
    c[ImGuiCol_PlotHistogram]         = blue;
    c[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.280f, 0.650f, 0.950f, 1.00f);
}

static const char *s_present_modes[] = {"mailbox", "fifo", "fifo_relaxed", "immediate"};
static const int s_fps_steps[] = {0, 30, 60, 90, 120, 144, 165};
static bool s_recreate_pending;
static const char *s_present_pending;

void ac_menu_apply_pending(void) {  // (ac_overlay.cpp pre_frame: between presents)
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
    if (!m) m = "mailbox";  // (swapchain.c default)
    for (int i = 0; i < 4; i++) if (!strcmp(m, s_present_modes[i])) return i;
    return !strcmp(m, "fifo-relaxed") ? 2 : 0;
}

static void section_performance(void) {
    ImGuiIO &io = ImGui::GetIO();
    static float hist[120];
    static int pos, filled;
    hist[pos] = io.Framerate;
    pos = (pos + 1) % 120;
    if (filled < 120) filled++;
    if (ImGui::CollapsingHeader("Desempenho", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%.1f FPS  (%.2f ms)%s", io.Framerate, io.Framerate > 0 ? 1000.0 / io.Framerate : 0,
                    (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) ? " [FSR 3.1 FG 2x]" : "");
        ImGui::PlotLines("##fps", hist, filled, 0, nullptr, 0.0f, 200.0f, ImVec2(-1, 60));
        ImGui::Text("Present mode: %s", s_present_modes[present_index()]);
        int fps = g_extra_fps_override >= 0 ? g_extra_fps_override : 60;
        if (fps > 0) ImGui::Text("FPS alvo: %d%s", fps, g_extra_fps_override > 0 ? "" : " (AC_FPS/env)");
        else ImGui::Text("FPS alvo: Ilimitado%s", g_extra_fps_override == 0 ? "" : " (AC_FPS/env)");
    }
}

static void section_graphics(void) {
    if (!ImGui::CollapsingHeader("Graficos", ImGuiTreeNodeFlags_DefaultOpen)) return;

    int aa = g_settings.antialias;
    if (ImGui::Combo("Antialiasing", &aa, "Off\0SMAA\0")) {
        g_settings.antialias = aa;
        printf("[overlay] antialias -> %s\n", aa == AA_SMAA ? "smaa" : "off");
    }
    int up = g_settings.upscaler == UPSCALE_FSR3 ? 2 : (g_settings.upscaler == UPSCALE_FSR1 ? 1 : 0);
    if (ImGui::Combo("Upscaler", &up, "Off (Bilinear)\0AMD FSR 1.0 (Spatial)\0AMD FSR 3.1 (Temporal + Jitter + RCAS)\0")) {
        g_settings.upscaler = (up == 2) ? UPSCALE_FSR3 : (up == 1 ? UPSCALE_FSR1 : UPSCALE_BILINEAR);
        settings_save();
        printf("[overlay] upscaler -> %s\n", up == 2 ? "fsr3.1" : (up == 1 ? "fsr1" : "bilinear"));
    }
    static const float s_scales[] = {0.50f, 0.58f, 0.67f, 0.75f, 1.00f, 1.25f, 1.50f, 2.00f};
    float cur_sc = g_extra_render_scale > 0.0f ? g_extra_render_scale : (g_settings.render_scale > 0.0f ? g_settings.render_scale : 1.0f);
    int sc_idx = 4;
    for (int i = 0; i < 8; i++) if (cur_sc >= s_scales[i] - 0.03f && cur_sc <= s_scales[i] + 0.03f) sc_idx = i;
    if (ImGui::Combo("Escala / Qualidade FSR", &sc_idx,
                     "0.50x (Performance)\00.58x (Balanced)\00.67x (Quality)\00.75x (Ultra Quality)\0"
                     "1.00x (Native AA)\01.25x (SSAA)\01.50x (SSAA)\02.00x (SSAA)\0")) {
        g_extra_render_scale = g_settings.render_scale = s_scales[sc_idx];
        settings_save();
        printf("[overlay] render scale -> %.2fx\n", s_scales[sc_idx]);
    }
    bool jit = (g_extra_fsr_jitter >= 0 ? g_extra_fsr_jitter : g_settings.fsr_jitter) != 0;
    if (ImGui::Checkbox("Camera Jitter Sub-Pixel (Halton 2,3)", &jit)) {
        g_extra_fsr_jitter = g_settings.fsr_jitter = jit ? 1 : 0;
        settings_save();
        printf("[overlay] fsr3 jitter -> %s\n", jit ? "on" : "off");
    }
    float shp = g_extra_fsr_sharpness >= 0.0f ? g_extra_fsr_sharpness : g_settings.fsr_sharpness;
    if (ImGui::SliderFloat("Nitidez FSR 3.1 (RCAS)", &shp, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_fsr_sharpness = g_settings.fsr_sharpness = shp;
        settings_save();
    }
    bool fg = (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) != 0;
    if (ImGui::Checkbox("FSR 3.1 Frame Generation (2x FPS)", &fg)) {
        g_extra_framegen = g_settings.framegen = fg ? 1 : 0;
        settings_save();
        printf("[overlay] fsr3 framegen -> %s\n", fg ? "on" : "off");
    }

    // Texture quality (global LOD bias) + LoD floor + shadow — all live overrides
    // over the LARECOMP_* env vars, applied at sampler build (textures.c vk_sampler).
    float bias = eff_lod_bias();
    if (ImGui::SliderFloat("Texturas (bias LOD)", &bias, 0.0f, 3.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_bias = bias;
        printf("[overlay] lod bias -> %.1f\n", bias);
    }
    float lod = eff_lod_min();
    if (ImGui::SliderFloat("LoD minimo", &lod, 0.0f, 4.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        g_extra_lod_min = lod;
        printf("[overlay] lod min -> %.1f\n", lod);
    }
    bool shadow = !eff_no_shadow();
    if (ImGui::Checkbox("Sombras", &shadow)) {
        g_extra_no_shadow = shadow ? 0 : 1;
        printf("[overlay] sombras -> %s\n", shadow ? "on" : "off");
    }

    int sel = 0;
    for (int i = 1; i < 7; i++) if (s_fps_steps[i] == g_extra_fps_override) sel = i;
    if (ImGui::Combo("Limite de FPS", &sel, "Env (AC_FPS)\0 30\0 60\0 90\0 120\0 144\0 165\0")) {
        g_extra_fps_override = s_fps_steps[sel];
        printf("[overlay] fps limit -> %d%s\n", s_fps_steps[sel], sel ? "" : " (env)");
        if (sel == 0) {
            const char *e = getenv("AC_FPS");
            if (e) printf("[overlay] env AC_FPS=%s\n", e);
        }
    }

    int pm = present_index();
    if (ImGui::Combo("VSync", &pm, "mailbox\0fifo (vsync estrito)\0fifo_relaxed\0immediate\0")) {
        s_present_pending = s_present_modes[pm];
        s_recreate_pending = true;  // swapchain recreation is deferred to ac_menu_apply_pending
    }
    ImGui::TextDisabled("VSync aplica ao vivo; demais itens valem no proximo frame.");
}

extern "C" int extra_get_guest_voice_language(void);
extern "C" void extra_set_guest_voice_language(int voice_lang);

static void section_game(void) {
    if (!ImGui::CollapsingHeader("Jogo & Idioma", ImGuiTreeNodeFlags_DefaultOpen)) return;

    static const char *const lang_items =
        "Auto (Sistema)\0"
        "Japones (ja)\0"
        "Ingles (en-US)\0"
        "Espanhol (es)\0"
        "Frances (fr)\0"
        "Alemao (de)\0"
        "Italiano (it)\0"
        "Holandes (nl)\0"
        "Russo (ru)\0"
        "Coreano (ko)\0"
        "Chines Simplificado (zh-CN)\0"
        "Chines Tradicional (zh-TW)\0"
        "Portugues (Brasil) (pt-BR)\0";

    int cur_lang = (g_settings.language >= 0 && g_settings.language < N_LANGUAGES) ? g_settings.language : 0;
    if (ImGui::Combo("Idioma dos Textos", &cur_lang, lang_items)) {
        g_settings.language = cur_lang;
        settings_save();
        printf("[overlay] idioma -> %s (%s)\n", g_language_codes[cur_lang], g_language_names[cur_lang]);
    }
    ImGui::TextDisabled("Ativo: %s (%s)", settings_language(), settings_language_name());
    ImGui::TextDisabled("Textos carregam no boot; reinicie o jogo para aplicar.");

    ImGui::Spacing();
    static const char *const voice_items =
        "Auto (Acompanha Idioma)\0"
        "Japones (ja)\0"
        "Ingles (en-US)\0";
    int cur_voice = (g_settings.voice_language >= 0 && g_settings.voice_language <= 2) ? g_settings.voice_language : 0;
    if (ImGui::Combo("Idioma das Vozes", &cur_voice, voice_items)) {
        g_settings.voice_language = cur_voice;
        settings_save();
        if (cur_voice == 1) extra_set_guest_voice_language(0);
        else if (cur_voice == 2) extra_set_guest_voice_language(1);
        else if (cur_voice == 0) extra_set_guest_voice_language(-1);
        printf("[overlay] vozes -> %s\n", cur_voice == 1 ? "ja" : cur_voice == 2 ? "en" : "auto");
    }
    int g_v = extra_get_guest_voice_language();
    ImGui::TextDisabled("Vozes no motor: %s", g_v == 0 ? "Japones (ja)" : g_v == 1 ? "Ingles (en-US)" : "Aguardando boot");
}

static void section_debug(void) {
    if (!ImGui::CollapsingHeader("Depuracao (Debug)", ImGuiTreeNodeFlags_DefaultOpen)) return;

    bool guard = eff_guard();
    if (ImGui::Checkbox("Vertex guard (clamp de vertex explosion)", &guard)) {
        g_extra_vtx_guard = guard ? 1 : 0;
        printf("[overlay] vtx guard -> %d\n", guard);
    }
    bool dbg = eff_debug();
    if (ImGui::Checkbox("Vertex debug logging (NaN/Inf em stderr)", &dbg)) {
        g_extra_vtx_debug = dbg ? 1 : 0;
        printf("[overlay] vtx debug -> %d\n", dbg);
    }

    bool async_env = env_on("AC_ASYNC_SUBMIT", nullptr);
    bool async_on = async_env && !g_extra_async_kill;
    if (!async_env) ImGui::BeginDisabled();
    if (ImGui::Checkbox("Async submit (worker de submissao)", &async_on)) {
        if (!async_on) {
            extra_async_submit_kill();  // one-way: re-enable needs a relaunch via launcher
            printf("[overlay] async submit killed (drained); reative pelo launcher\n");
        }
    }
    if (!async_env) ImGui::EndDisabled();
    if (!async_env)
        ImGui::TextDisabled("Off no launcher; ligar requer relaunch (worker).");
    else if (g_extra_async_kill)
        ImGui::TextDisabled("Killed nesta sessao; reative pelo launcher.");

    ImGui::Checkbox("GPU profiler (g_gpuprof_frame)", &g_gpuprof_frame);
    if (ImGui::Button("Dump debug frame (F9)")) {
        g_debug_frame_request = 1;
        printf("[overlay] debug frame dump requested\n");
    }
    bool reinfo = ac_reinfo_visible() != 0;
    if (ImGui::Checkbox("Janela RE Info (F8)", &reinfo)) ac_reinfo_toggle();
    ImGui::TextDisabled("A/B do flicker: desligue Async submit e observe.");
}

static void section_discord(void) {
    if (!ImGui::CollapsingHeader("Discord Rich Presence", ImGuiTreeNodeFlags_DefaultOpen)) return;
    AcOverlayRpc::RenderSection();
}

static void section_about(void) {
    if (!ImGui::CollapsingHeader("Sobre")) return;
    ImGui::Text("Astral Chain PC (Linux First)");
    ImGui::Text("GPU: %s", ac_vk_gpu_name());
    ImGui::Text("ImGui %s", ImGui::GetVersion());
    ImGui::Separator();
    ImGui::BulletText("Insert: abre/fecha este menu");
    ImGui::BulletText("F8: janela RE Info (valores de engenharia reversa)");
    ImGui::BulletText("F9: dump do frame | F10: captura do mouse");
    ImGui::BulletText("F11: tela cheia | Alt+F4: sair");
}

void ac_menu_frame(void) {
    ImGui::SetNextWindowSize(ImVec2(460, 740), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::Begin("Astral Chain PC", nullptr, ImGuiWindowFlags_NoCollapse);
    section_performance();
    section_graphics();
    section_game();
    section_discord();
    section_debug();
    section_about();
    ImGui::End();
}
