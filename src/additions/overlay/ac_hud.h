// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Astral Chain "IRIS // Legatus" ImGui theme, tactical HUD primitives,
// and in-game Performance Overlay (visible when the main Insert menu is hidden).
#pragma once

#include "imgui.h"

namespace AcIrisColors {
inline constexpr ImVec4 Cyan       = ImVec4(0.00f, 0.83f, 1.00f, 1.00f); // #00D4FF (IRIS primary)
inline constexpr ImVec4 CyanBright = ImVec4(0.35f, 0.91f, 1.00f, 1.00f); // #59E8FF
inline constexpr ImVec4 CyanDim    = ImVec4(0.00f, 0.58f, 0.78f, 0.85f);
inline constexpr ImVec4 Cobalt     = ImVec4(0.19f, 0.41f, 0.72f, 1.00f); // #3168B8 (Neuron blue)
inline constexpr ImVec4 CobaltHi   = ImVec4(0.25f, 0.50f, 0.85f, 1.00f);
inline constexpr ImVec4 Crimson    = ImVec4(1.00f, 0.18f, 0.34f, 1.00f); // #FF2E57 (Redshift alert)
inline constexpr ImVec4 Amber      = ImVec4(1.00f, 0.72f, 0.15f, 1.00f); // #FFB826
inline constexpr ImVec4 Emerald    = ImVec4(0.15f, 0.92f, 0.55f, 1.00f); // #26EB8C
inline constexpr ImVec4 TextMain   = ImVec4(0.89f, 0.95f, 0.98f, 1.00f); // #E3F2FA
inline constexpr ImVec4 TextMuted  = ImVec4(0.49f, 0.60f, 0.71f, 1.00f); // #7D99B5
inline constexpr ImVec4 BgCard     = ImVec4(0.06f, 0.10f, 0.17f, 0.88f); // #0F1A2B
} // namespace AcIrisColors

void ac_ui_apply_iris_style(void);

inline void ac_ui_draw_iris_corners(ImVec2 p_min, ImVec2 p_max, ImU32 col, float len = 8.0f, float thick = 1.8f) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(p_min.x, p_min.y + len), p_min, col, thick);
    dl->AddLine(p_min, ImVec2(p_min.x + len, p_min.y), col, thick);
    dl->AddLine(ImVec2(p_max.x - len, p_min.y), ImVec2(p_max.x, p_min.y), col, thick);
    dl->AddLine(ImVec2(p_max.x, p_min.y), ImVec2(p_max.x, p_min.y + len), col, thick);
    dl->AddLine(ImVec2(p_min.x, p_max.y - len), ImVec2(p_min.x, p_max.y), col, thick);
    dl->AddLine(ImVec2(p_min.x, p_max.y), ImVec2(p_min.x + len, p_max.y), col, thick);
    dl->AddLine(ImVec2(p_max.x - len, p_max.y), p_max, col, thick);
    dl->AddLine(p_max, ImVec2(p_max.x, p_max.y - len), col, thick);
}

inline void ac_ui_draw_window_chrome(void) {
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
    ImVec2 p_max(wp.x + ws.x, wp.y + ws.y);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(wp.x + 1.0f, wp.y + 1.0f), ImVec2(p_max.x - 1.0f, wp.y + 1.0f),
                                        IM_COL32(0, 212, 255, 190), 2.0f);
    ac_ui_draw_iris_corners(wp, p_max, IM_COL32(0, 225, 255, 235), 9.0f, 2.0f);
}

inline void ac_ui_section_header(const char *title, const char *sys_tag = nullptr) {
    ImGui::Spacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetTextLineHeight() + 6.0f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(18, 36, 62, 185), 2.0f);
    dl->AddRectFilled(p, ImVec2(p.x + 3.5f, p.y + h), IM_COL32(0, 212, 255, 255), 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 10.0f, p.y + 3.0f));
    ImGui::TextColored(AcIrisColors::CyanBright, "%s", title);
    if (sys_tag) {
        float tw = ImGui::CalcTextSize(sys_tag).x;
        ImGui::SameLine(w - tw - 6.0f);
        ImGui::TextColored(AcIrisColors::TextMuted, "%s", sys_tag);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 4.0f));
}

void ac_perf_hud_tick(void);
void ac_perf_hud_render(bool menu_visible);
void ac_perf_hud_dashboard_section(const char *present_mode);
