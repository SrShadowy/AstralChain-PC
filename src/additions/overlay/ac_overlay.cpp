// [extra] In-game overlay: ImGui context + SDL/Vulkan backend wiring.
// The menu itself lives in ac_menu.cpp; this file owns context lifetime,
// event feeding and the per-frame entry points used by postprocess.c hooks.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "settings.h"

#include "additions/config/ac_config_sync.h"
#include "additions/overlay/ac_hud.h"
#include "additions/overlay/ac_overlay.h"
#include "additions/overlay/ac_overlay_rpc.h"
#include "additions/overlay/ac_reinfo.h"

extern "C" bool ac_vk_imgui_init(void);
extern "C" void ac_vk_imgui_new_frame(void);
extern "C" void ac_vk_imgui_render(void *cmd);
void ac_menu_frame(void);
void ac_menu_apply_pending(void);      // (ac_menu.cpp: deferred swapchain work)

static SDL_Window *s_win;
static bool s_ctx_ok;
static bool s_vk_ok;
static bool s_visible;

static void overlay_load_font(ImGuiIO &io) {
    static const char *const k_fonts[] = {
        "/usr/share/fonts/TTF/FiraSans-Medium.ttf",
        "/usr/share/fonts/TTF/FiraSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Medium.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
    for (size_t i = 0; i < sizeof(k_fonts) / sizeof(k_fonts[0]); i++) {
        FILE *f = fopen(k_fonts[i], "rb");
        if (f) {
            fclose(f);
            if (io.Fonts->AddFontFromFileTTF(k_fonts[i], 15.5f)) return;
        }
    }
    io.Fonts->AddFontDefault();
}

static void overlay_ensure_context(void) {
    if (s_ctx_ok || !s_win) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // no on-disk imgui state: the env stays the source of truth
    overlay_load_font(io);
    ImGui::StyleColorsDark();
    ac_ui_apply_iris_style();
    s_ctx_ok = ImGui_ImplSDL3_InitForVulkan(s_win);
    if (!s_ctx_ok) fprintf(stderr, "[overlay] ImGui SDL3 backend init failed\n");
}

extern "C" void ac_overlay_set_window(void *sdl_window) {
    s_win = (SDL_Window *)sdl_window;
    overlay_ensure_context();
}

extern "C" void ac_overlay_process_event(const void *sdl_event) {
    if (s_ctx_ok) ImGui_ImplSDL3_ProcessEvent((const SDL_Event *)sdl_event);
}

static void overlay_boot_state(void) {  // AC_OVERLAY=1 starts with the menu open (CI/QA screenshots)
    static bool done;
    if (done) return;
    done = true;
    const char *e = getenv("AC_OVERLAY");
    if (e && atoi(e) != 0) s_visible = true;
}

extern "C" int ac_overlay_visible(void) {
    overlay_boot_state();
    return s_visible ? 1 : 0;
}

extern "C" void ac_overlay_toggle(void) {
    s_visible = !s_visible;
    printf("[overlay] in-game menu %s (Insert / L3+R3 2s)\n", s_visible ? "open" : "closed");
    fflush(stdout);
}

extern "C" int extra_get_guest_voice_language(void);
extern "C" void extra_set_guest_voice_language(int voice_lang);

static void sync_voice_language(void) {
    static int s_last_desired = -1;
    static int s_attempts = 0;

    int setting = g_settings.voice_language;
    int desired = (setting == 1) ? 0
                : (setting == 2) ? 1
                : (!strncmp(settings_language(), "ja", 2) ? 0 : 1);
    if (desired != s_last_desired) {
        s_last_desired = desired;
        s_attempts = 0;
    }
    if (s_attempts >= 600) {
        return;
    }
    int current = extra_get_guest_voice_language();
    if (current != desired) {
        s_attempts++;
        extra_set_guest_voice_language(desired);
    }
}

extern "C" void ac_overlay_pre_frame(void) {
    ac_config_sync_poll();
    ac_menu_apply_pending();
    sync_voice_language();
}

extern "C" void ac_overlay_frame(void) {
    overlay_boot_state();
    if (!s_ctx_ok) {
        return;
    }
    if (!s_vk_ok) {
        s_vk_ok = ac_vk_imgui_init();
        if (!s_vk_ok) {
            s_ctx_ok = false;
            return;
        }
    }
    ImGuiIO &io = ImGui::GetIO();
    if (s_visible || ac_reinfo_visible()) {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }
    ac_vk_imgui_new_frame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ac_perf_hud_tick();
    ac_perf_hud_render(s_visible);
    if (s_visible) {
        ac_menu_frame();
    }
    ac_reinfo_frame();  // [extra] its own window and toggle (AC_REINFO=0 starts closed)
    AcOverlayRpc::RenderWindow();  // [extra] Discord Rich Presence config and preview card
    ImGui::Render();
    extern void *vk_frame_cmd(void);  // (device/present.c: current submission's command buffer)
    ac_vk_imgui_render(vk_frame_cmd());
}
