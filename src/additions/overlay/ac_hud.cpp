// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Astral Chain "IRIS // Legatus" ImGui theme, fixed-slot tabular digits,
// and in-game Performance Overlay (active when the Insert menu is hidden).
#include "extra/ui/overlay/ac_hud.h"

#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "extra/toggles/ac_toggles.h"

namespace {

constexpr int kHistLen = 120;
float s_fps_hist[kHistLen];
float s_ms_hist[kHistLen];
int   s_hist_pos = 0;
int   s_hist_filled = 0;
float s_low1_fps = 0.0f;

// Smoothed display readout latched every ~120 ms so digits don't blur at high FPS.
float  s_disp_fps = 0.0f;
float  s_disp_ms = 0.0f;
float  s_disp_low1 = 0.0f;
double s_last_latch_time = -1.0;

bool  s_hud_booted = false;
bool  s_hud_enabled = false;
int   s_hud_mode = 0;            // 0 = Compacto, 1 = Detalhado, 2 = Mínimo
int   s_hud_corner = 1;          // 0 = Sup. Esq, 1 = Sup. Dir, 2 = Inf. Esq, 3 = Inf. Dir
float s_hud_opacity = 0.86f;
bool  s_hud_show_with_menu = true;

int effective_target_fps(void) {
    if (g_extra_fps_override > 0) return g_extra_fps_override;
    if (g_extra_fps_override == 0) {
        const char *e = getenv("AC_FPS");
        if (e && *e) return atoi(e);
    }
    return 60;
}

bool effective_fg(void) {
    return (g_extra_framegen >= 0 ? g_extra_framegen : g_settings.framegen) != 0;
}

float effective_scale(void) {
    if (g_extra_render_scale > 0.0f) return g_extra_render_scale;
    return g_settings.render_scale > 0.0f ? g_settings.render_scale : 1.0f;
}

const char *upscaler_short_label(void) {
    if (g_settings.upscaler == UPSCALE_FSR3) return effective_fg() ? "FSR 3.1 + FG 2x" : "FSR 3.1";
    if (g_settings.upscaler == UPSCALE_FSR1) return "FSR 1.0";
    return effective_fg() ? "Bilinear + FG 2x" : "Nativo";
}

ImVec4 fps_health_color(float fps) {
    int target = effective_target_fps();
    if (target <= 0) target = 60;
    if (fps >= 58.0f || fps >= target * 0.90f) return AcIrisColors::Emerald;
    if (fps >= 40.0f || fps >= target * 0.65f) return AcIrisColors::Amber;
    return AcIrisColors::Crimson;
}

// Measures string width treating every ASCII digit '0'..'9' as having the width of '8'.
float calc_tabular_width(const char *s) {
    ImFont *font = ImGui::GetFont();
    const float sz = ImGui::GetFontSize();
    const float w8 = ImGui::CalcTextSize("8").x;
    float total = 0.0f;
    for (const char *p = s; *p; ) {
        if (*p >= '0' && *p <= '9') {
            total += w8;
            p++;
        } else {
            const char *next = p + 1;
            while (*next && (*next & 0xC0) == 0x80) next++;
            total += font->CalcTextSizeA(sz, FLT_MAX, 0.0f, p, next).x;
            p = next;
        }
    }
    return total;
}

// Draws formatted text inside a fixed-width box sized to `max_template` (e.g. "8888.8 FPS"),
// rendering each digit '0'..'9' centered in a cell of width '8' so width never jitters.
void draw_fixed_slot(const char *max_template, ImVec4 col, const char *fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImFont *font = ImGui::GetFont();
    const float sz = ImGui::GetFontSize();
    const float line_h = ImGui::GetTextLineHeight();
    const float w8 = ImGui::CalcTextSize("8").x;
    const float slot_w = calc_tabular_width(max_template);
    const float text_w = calc_tabular_width(buf);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float x = pos.x + (slot_w > text_w ? (slot_w - text_w) : 0.0f);
    const ImU32 ucol = ImGui::ColorConvertFloat4ToU32(col);

    for (const char *p = buf; *p; ) {
        if (*p >= '0' && *p <= '9') {
            char d[2] = {*p, '\0'};
            float dw = font->CalcTextSizeA(sz, FLT_MAX, 0.0f, d, d + 1).x;
            dl->AddText(font, sz, ImVec2(x + (w8 - dw) * 0.5f, pos.y), ucol, d, d + 1);
            x += w8;
            p++;
        } else {
            const char *next = p + 1;
            while (*next && (*next & 0xC0) == 0x80) next++;
            float cw = font->CalcTextSizeA(sz, FLT_MAX, 0.0f, p, next).x;
            dl->AddText(font, sz, ImVec2(x, pos.y), ucol, p, next);
            x += cw;
            p = next;
        }
    }
    ImGui::Dummy(ImVec2(slot_w, line_h));
}

} // namespace

void ac_ui_apply_iris_style(void) {
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = style.ChildRounding = style.PopupRounding = 3.0f;
    style.FrameRounding = style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 2.0f;
    style.WindowBorderSize = style.ChildBorderSize = style.FrameBorderSize = style.PopupBorderSize = style.TabBorderSize = 1.0f;
    style.WindowPadding    = ImVec2(14.0f, 12.0f);
    style.FramePadding     = ImVec2(9.0f, 5.0f);
    style.ItemSpacing      = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.CellPadding      = ImVec2(8.0f, 5.0f);
    style.ScrollbarSize    = 11.0f;
    style.GrabMinSize      = 9.0f;

    const ImVec4 bg_win   = ImVec4(0.039f, 0.063f, 0.106f, 0.95f); // #0A101B
    const ImVec4 bg_child = ImVec4(0.055f, 0.086f, 0.145f, 0.78f); // #0E1625
    const ImVec4 bg_popup = ImVec4(0.051f, 0.082f, 0.137f, 0.98f); // #0D1523
    const ImVec4 bg_frame = ImVec4(0.075f, 0.122f, 0.200f, 0.92f); // #131F33
    const ImVec4 bg_hover = ImVec4(0.118f, 0.204f, 0.337f, 0.95f); // #1E3456
    const ImVec4 border   = ImVec4(0.000f, 0.780f, 1.000f, 0.30f); // IRIS cyan border

    ImVec4 *c = style.Colors;
    c[ImGuiCol_Text]                 = AcIrisColors::TextMain;
    c[ImGuiCol_TextDisabled]         = AcIrisColors::TextMuted;
    c[ImGuiCol_WindowBg]             = bg_win;
    c[ImGuiCol_ChildBg]              = bg_child;
    c[ImGuiCol_PopupBg]              = bg_popup;
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg]              = bg_frame;
    c[ImGuiCol_FrameBgHovered]       = bg_hover;
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.165f, 0.310f, 0.510f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.031f, 0.051f, 0.086f, 0.98f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.067f, 0.129f, 0.227f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.031f, 0.051f, 0.086f, 0.80f);
    c[ImGuiCol_MenuBarBg]            = bg_child;
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.031f, 0.051f, 0.086f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.165f, 0.310f, 0.510f, 0.85f);
    c[ImGuiCol_ScrollbarGrabHovered] = AcIrisColors::CobaltHi;
    c[ImGuiCol_ScrollbarGrabActive]  = AcIrisColors::Cyan;
    c[ImGuiCol_CheckMark]            = c[ImGuiCol_SliderGrab] = AcIrisColors::Cyan;
    c[ImGuiCol_SliderGrabActive]     = AcIrisColors::CyanBright;
    c[ImGuiCol_Button]               = ImVec4(0.118f, 0.235f, 0.412f, 0.92f);
    c[ImGuiCol_ButtonHovered]        = AcIrisColors::CobaltHi;
    c[ImGuiCol_ButtonActive]         = AcIrisColors::CyanDim;
    c[ImGuiCol_Header]               = ImVec4(0.102f, 0.200f, 0.349f, 0.85f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.149f, 0.290f, 0.498f, 0.92f);
    c[ImGuiCol_HeaderActive]         = AcIrisColors::CobaltHi;
    c[ImGuiCol_Separator]            = ImVec4(0.000f, 0.780f, 1.000f, 0.25f);
    c[ImGuiCol_SeparatorHovered]     = AcIrisColors::Cyan;
    c[ImGuiCol_SeparatorActive]      = AcIrisColors::CyanBright;
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.000f, 0.831f, 1.000f, 0.25f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.000f, 0.831f, 1.000f, 0.65f);
    c[ImGuiCol_ResizeGripActive]     = AcIrisColors::Cyan;
    c[ImGuiCol_Tab]                  = ImVec4(0.059f, 0.102f, 0.173f, 0.92f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.165f, 0.325f, 0.565f, 0.95f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.137f, 0.282f, 0.498f, 1.00f);
    c[ImGuiCol_TabDimmed]            = ImVec4(0.043f, 0.075f, 0.125f, 0.85f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.102f, 0.204f, 0.361f, 0.95f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.082f, 0.153f, 0.263f, 0.95f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.000f, 0.780f, 1.000f, 0.32f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.000f, 0.780f, 1.000f, 0.14f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.039f, 0.067f, 0.114f, 0.55f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.067f, 0.114f, 0.188f, 0.55f);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.000f, 0.831f, 1.000f, 0.28f);
    c[ImGuiCol_NavCursor]            = c[ImGuiCol_PlotLines] = c[ImGuiCol_PlotHistogramHovered] = AcIrisColors::Cyan;
    c[ImGuiCol_PlotLinesHovered]     = AcIrisColors::CyanBright;
    c[ImGuiCol_PlotHistogram]        = AcIrisColors::CobaltHi;
}

void ac_perf_hud_tick(void) {
    if (!s_hud_booted) {
        s_hud_booted = true;
        const char *e = getenv("AC_PERF_HUD");
        if (e && atoi(e) > 0) {
            s_hud_enabled = true;
            int m = atoi(e);
            if (m >= 1 && m <= 3) s_hud_mode = (m == 1) ? 0 : (m == 2 ? 1 : 2);
        }
    }
    ImGuiIO &io = ImGui::GetIO();
    float fps = io.Framerate;
    float ms  = fps > 0.01f ? (1000.0f / fps) : 0.0f;
    s_fps_hist[s_hist_pos] = fps;
    s_ms_hist[s_hist_pos]  = ms;
    s_hist_pos = (s_hist_pos + 1) % kHistLen;
    if (s_hist_filled < kHistLen) s_hist_filled++;

    float max_ms = 0.0f;
    for (int i = 0; i < s_hist_filled; i++) {
        if (s_ms_hist[i] > max_ms) max_ms = s_ms_hist[i];
    }
    s_low1_fps = max_ms > 0.01f ? (1000.0f / max_ms) : fps;

    double now = ImGui::GetTime();
    if (s_last_latch_time < 0.0 || (now - s_last_latch_time) >= 0.12) {
        s_disp_fps  = fps;
        s_disp_ms   = ms;
        s_disp_low1 = s_low1_fps;
        s_last_latch_time = now;
    }
}

void ac_perf_hud_render(bool menu_visible) {
    if (!s_hud_enabled) return;
    if (menu_visible && !s_hud_show_with_menu) return;

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    const float pad = 16.0f;
    ImVec2 pos((s_hud_corner & 1) ? (vp->WorkPos.x + vp->WorkSize.x - pad) : (vp->WorkPos.x + pad),
               (s_hud_corner & 2) ? (vp->WorkPos.y + vp->WorkSize.y - pad) : (vp->WorkPos.y + pad));
    ImVec2 pivot((s_hud_corner & 1) ? 1.0f : 0.0f, (s_hud_corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(s_hud_opacity);

    constexpr ImGuiWindowFlags kHudFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(11.0f, 8.0f));
    if (ImGui::Begin("##ac_iris_perf_hud", nullptr, kHudFlags)) {
        ac_ui_draw_window_chrome();
        ImVec4 fcol = fps_health_color(s_disp_fps);

        if (s_hud_mode == 2) {
            draw_fixed_slot("● 8888 FPS", fcol, "● %.0f FPS", s_disp_fps);
            ImGui::SameLine(0.0f, 8.0f);
            draw_fixed_slot("888.8 ms", AcIrisColors::TextMuted, "%.1f ms", s_disp_ms);
        } else if (s_hud_mode == 0) {
            ImGui::TextColored(AcIrisColors::Cyan, "IRIS //");
            ImGui::SameLine(0.0f, 6.0f);
            draw_fixed_slot("8888.8 FPS", fcol, "%.1f FPS", s_disp_fps);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 8.0f);
            draw_fixed_slot("888.88 ms", AcIrisColors::TextMain, "%.2f ms", s_disp_ms);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 8.0f);
            draw_fixed_slot("1% 8888", AcIrisColors::TextMuted, "1%% %.0f", s_disp_low1);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(AcIrisColors::CyanBright, "%s", upscaler_short_label());
        } else {
            ImGui::TextColored(AcIrisColors::Cyan, "IRIS // TELEMETRY");
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextColored(AcIrisColors::CyanBright, "[%s · %.2fx]", upscaler_short_label(), effective_scale());
            ImGui::Separator();
            draw_fixed_slot("● 8888.8 FPS", fcol, "● %.1f FPS", s_disp_fps);
            ImGui::SameLine(0.0f, 12.0f);
            draw_fixed_slot("888.88 ms", AcIrisColors::TextMain, "%.2f ms", s_disp_ms);
            ImGui::SameLine(0.0f, 12.0f);
            draw_fixed_slot("1% Low: 8888 FPS", AcIrisColors::TextMuted, "1%% Low: %.0f FPS", s_disp_low1);
            ImGui::PlotLines("##hud_ms", s_ms_hist, s_hist_filled, s_hist_pos, nullptr, 0.0f, 35.0f, ImVec2(270.0f, 36.0f));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void ac_perf_hud_dashboard_section(const char *present_mode) {
    ac_ui_section_header("TELEMETRIA EM TEMPO REAL", "// SYS.PERF");
    if (ImGui::BeginTable("##perf_cards", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("TAXA DE QUADROS");
        draw_fixed_slot("8888.8 FPS", fps_health_color(s_disp_fps), "%.1f FPS", s_disp_fps);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("FRAMETIME");
        draw_fixed_slot("888.88 ms", AcIrisColors::TextMain, "%.2f ms", s_disp_ms);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("1%% LOW (ESTAB.)");
        draw_fixed_slot("8888.8 FPS", AcIrisColors::CyanBright, "%.1f FPS", s_disp_low1);
        ImGui::TableSetColumnIndex(3);
        ImGui::TextDisabled("PIPELINE / VSYNC");
        ImGui::Text("%s (%s)", upscaler_short_label(), present_mode);
        ImGui::EndTable();
    }

    ImGui::PlotLines("##fps_graph", s_fps_hist, s_hist_filled, s_hist_pos,
                     "Histórico de FPS (120 quadros)", 0.0f, 180.0f, ImVec2(-1.0f, 56.0f));
    ImGui::PlotLines("##ms_graph", s_ms_hist, s_hist_filled, s_hist_pos,
                     "Tempo de Quadro em ms (menor = mais estável)", 0.0f, 40.0f, ImVec2(-1.0f, 46.0f));

    ac_ui_section_header("OVERLAY DE PERFORMANCE (MENU ESCONDIDO)", "// HUD.IRIS");
    ImGui::Checkbox("Ativar Overlay de Performance em Jogo (Menu Oculto)", &s_hud_enabled);
    ImGui::BeginDisabled(!s_hud_enabled);
    ImGui::Combo("Estilo do HUD", &s_hud_mode,
                 "Compacto (Barra Tática IRIS)\0Detalhado (Telemetria + Gráfico)\0Mínimo (Pílula FPS)\0");
    ImGui::Combo("Posição na Tela", &s_hud_corner,
                 "Superior Esquerdo\0Superior Direito\0Inferior Esquerdo\0Inferior Direito\0");
    ImGui::SliderFloat("Opacidade do HUD", &s_hud_opacity, 0.30f, 1.00f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Manter visível enquanto este menu estiver aberto (Prévia)", &s_hud_show_with_menu);
    ImGui::EndDisabled();
    ImGui::TextDisabled("Dica: Feche este painel com [Insert] para jogar com o HUD ativo sem bloquear o controle.");
}
