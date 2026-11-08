// [extra] In-game overlay: ImGui context + SDL/Vulkan backend wiring.
// The menu itself lives in ac_menu.cpp; this file owns context lifetime,
// event feeding and the per-frame entry points used by postprocess.c hooks.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "settings.h"

#include "extra/ui/overlay/ac_overlay.h"
#include "extra/ui/overlay/ac_overlay_rpc.h"
#include "extra/ui/overlay/ac_reinfo.h"

extern "C" bool ac_vk_imgui_init(void);
extern "C" void ac_vk_imgui_new_frame(void);
extern "C" void ac_vk_imgui_render(void *cmd);
void ac_menu_frame(void);
void ac_menu_apply_style(void);        // (ac_menu.cpp: bbport dark theme)
void ac_menu_apply_pending(void);      // (ac_menu.cpp: deferred swapchain work)

static SDL_Window *s_win;
static bool s_ctx_ok;
static bool s_vk_ok;
static bool s_visible;

static void overlay_ensure_context(void) {
    if (s_ctx_ok || !s_win) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // no on-disk imgui state: the env stays the source of truth
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ac_menu_apply_style();
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
    printf("[overlay] in-game menu %s (Insert)\n", s_visible ? "open" : "closed");
    fflush(stdout);
}

extern "C" int extra_get_guest_voice_language(void);
extern "C" void extra_set_guest_voice_language(int voice_lang);

static void apply_boot_voice_language(void) {
    if (g_settings.voice_language <= 0) return;
    static int attempts = 0;
    if (attempts >= 600) return;
    attempts++;

    int desired = (g_settings.voice_language == 1) ? 0 : 1;
    int current = extra_get_guest_voice_language();
    if (current != desired) {
        extra_set_guest_voice_language(desired);
    }
}

extern "C" void ac_overlay_pre_frame(void) {
    ac_menu_apply_pending();
    apply_boot_voice_language();
}

extern "C" void ac_overlay_frame(void) {
    overlay_boot_state();
    if (!s_ctx_ok) return;
    if (!s_vk_ok) {
        s_vk_ok = ac_vk_imgui_init();
        if (!s_vk_ok) { s_ctx_ok = false; return; }  // backend refused: stay hidden for good
    }
    ac_vk_imgui_new_frame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (s_visible) ac_menu_frame();
    ac_reinfo_frame();  // [extra] its own window and toggle (AC_REINFO=0 starts closed)
    AcOverlayRpc::RenderWindow();  // [extra] Discord Rich Presence config and preview card
    ImGui::Render();
    extern void *vk_frame_cmd(void);  // (device/present.c: current submission's command buffer)
    ac_vk_imgui_render(vk_frame_cmd());
}
