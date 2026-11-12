// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] ImGui section, configuration window and Discord live preview card.
#include "additions/overlay/ac_overlay_rpc.h"

#include <cstring>
#include "imgui.h"
#include "additions/rpc/ac_discord_rpc.h"
#include "additions/rpc/ac_game_data.h"
#include "additions/overlay/ac_hud.h"

namespace AcOverlayRpc {
namespace {

void CheckboxAtomic(const char* label, std::atomic<bool>& val) {
    bool v = val.load(std::memory_order_relaxed);
    if (ImGui::Checkbox(label, &v)) {
        val.store(v, std::memory_order_relaxed);
    }
}

void Hint(const char* text) {
    ImGui::SameLine();
    ImGui::TextColored(AcIrisColors::CyanDim, "[?]");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

ImVec4 GetStatusColor(bool enabled, bool connected) {
    if (!enabled) return AcIrisColors::TextMuted;
    if (connected) return AcIrisColors::Emerald;
    return AcIrisColors::Amber;
}

} // namespace

void RenderSection() {
    AcDiscordRpc::Init();
    auto& s = AcDiscordRpc::GetSettings();
    ac_ui_section_header("DISCORD RICH PRESENCE (RPC)", "// SYS.RPC");

    const bool enabled = s.enabled.load(std::memory_order_relaxed);
    const bool connected = AcDiscordRpc::IsConnected();
    ImGui::TextColored(GetStatusColor(enabled, connected), "● %s", AcDiscordRpc::GetStatusText());

    CheckboxAtomic("Ativar Discord Rich Presence", s.enabled);
    Hint("Exibe no seu perfil do Discord o capítulo (File), caso/missão, área atual e combate a bosses.");

    ImGui::BeginDisabled(!enabled);
    const bool win_open = s.window_open.load(std::memory_order_relaxed);
    const char* btn_text = win_open ? "Fechar Painel de Configuração do Discord###btn_ac_drpc_win"
                                    : "Abrir Painel de Configuração do Discord###btn_ac_drpc_win";

    ImGui::PushStyleColor(ImGuiCol_Button, win_open ? ImVec4(0.08f, 0.36f, 0.48f, 0.95f) : AcIrisColors::Cobalt);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, win_open ? AcIrisColors::CyanDim : AcIrisColors::CobaltHi);
    if (ImGui::Button(btn_text, ImVec2(-34.0f, 0))) {
        s.window_open.store(!win_open, std::memory_order_relaxed);
    }
    ImGui::PopStyleColor(2);
    Hint("Abre janela tática com opções granulares (Capítulo, Missão, Dificuldade, Boss, Área, Tempo) e prévia ao vivo.");
    ImGui::EndDisabled();
}

void RenderWindow() {
    auto& s = AcDiscordRpc::GetSettings();
    if (!s.window_open.load(std::memory_order_relaxed)) return;

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("IRIS // DISCORD RICH PRESENCE###ac_discord_rpc_win", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) s.window_open.store(false, std::memory_order_relaxed);
        return;
    }
    if (!open) {
        s.window_open.store(false, std::memory_order_relaxed);
        ImGui::End();
        return;
    }
    ac_ui_draw_window_chrome();

    const bool enabled = s.enabled.load(std::memory_order_relaxed);
    const bool connected = AcDiscordRpc::IsConnected();
    ImGui::TextColored(GetStatusColor(enabled, connected), "● %s", AcDiscordRpc::GetStatusText());
    ImGui::SameLine(0.0f, 20.0f);
    CheckboxAtomic("Ativar Presença", s.enabled);

    ImGui::BeginDisabled(!enabled);

    ac_ui_section_header("AGENTE & MISSÃO", "// RPC.FILE");
    CheckboxAtomic("Exibir Arquivo / Capítulo (File 01..12)", s.show_file);
    Hint("Exibe o número e título do capítulo atual (ex: 'File 03: Link').");

    CheckboxAtomic("Exibir Missão / Caso Atual", s.show_mission);
    Hint("Exibe o nome do caso ou missão atual (ex: 'Maison Forest Search').");

    CheckboxAtomic("Exibir Dificuldade", s.show_difficulty);
    Hint("Exibe a dificuldade selecionada (Standard, Casual, Hardcore...).");

    ac_ui_section_header("MUNDO & COMBATE", "// RPC.WORLD");
    CheckboxAtomic("Exibir Área / Localização Atual", s.show_area);
    Hint("Exibe a área atual (ex: 'Zone 09', 'Sector V', 'Astral Plane').");

    CheckboxAtomic("Exibir Combate contra Boss", s.show_boss);
    Hint("Mostra 'Lutando: <Nome do Boss>' na linha de estado quando em combate de boss.");

    ac_ui_section_header("SESSÃO & CLIENT ID", "// RPC.APP");
    CheckboxAtomic("Exibir Tempo de Sessão Decorrido", s.show_time);
    Hint("Mostra o cronômetro com tempo decorrido desde o início da sessão.");

    char cid_buf[64];
    std::strncpy(cid_buf, s.client_id.c_str(), sizeof(cid_buf) - 1);
    cid_buf[sizeof(cid_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("##ac_discord_cid", cid_buf, sizeof(cid_buf))) {
        s.client_id = cid_buf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Padrão (Astral Chain)")) {
        s.client_id = "1568955734606086196";
    }
    Hint("ID da aplicação Discord. Permite apontar para sua própria aplicação de Rich Presence no Developer Portal.");

    ImGui::EndDisabled();

    ac_ui_section_header("PRÉVIA EM TEMPO REAL NO DISCORD", "// PREVIEW");

    auto act = AcDiscordRpc::GetCurrentActivity();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, AcIrisColors::BgCard);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

    if (ImGui::BeginChild("##ac_discord_preview_card", ImVec2(0, 126.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar)) {
        ImVec2 cp = ImGui::GetWindowPos();
        ImVec2 cs = ImGui::GetWindowSize();
        ac_ui_draw_iris_corners(cp, ImVec2(cp.x + cs.x, cp.y + cs.y), IM_COL32(0, 212, 255, 180), 6.0f, 1.5f);
        ImGui::TextColored(AcIrisColors::Cyan, "JOGANDO UM JOGO");

        ImVec2 icon_pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(icon_pos, ImVec2(icon_pos.x + 50.0f, icon_pos.y + 50.0f), IM_COL32(16, 30, 52, 255), 3.0f);
        dl->AddRect(icon_pos, ImVec2(icon_pos.x + 50.0f, icon_pos.y + 50.0f), IM_COL32(0, 212, 255, 230), 3.0f, 0, 1.5f);
        dl->AddText(ImVec2(icon_pos.x + 15.0f, icon_pos.y + 16.0f), IM_COL32(0, 225, 255, 255), "AC");

        ImGui::Dummy(ImVec2(58.0f, 52.0f));
        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextColored(AcIrisColors::TextMain, "Astral Chain");

        if (!act.details.empty()) {
            ImGui::TextColored(AcIrisColors::CyanBright, "%s", act.details.c_str());
        }

        if (!act.state.empty()) {
            ImGui::TextColored(AcIrisColors::TextMain, "%s", act.state.c_str());
        }

        if (!act.small_text.empty()) {
            ImGui::TextColored(AcIrisColors::CyanDim, "[%s] %s", act.small_image.c_str(), act.small_text.c_str());
        }

        if (act.has_time && !act.time_text.empty()) {
            ImGui::TextColored(AcIrisColors::TextMuted, "%s", act.time_text.c_str());
        }
        ImGui::EndGroup();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
}

} // namespace AcOverlayRpc
