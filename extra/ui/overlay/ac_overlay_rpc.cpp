// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] ImGui section, configuration window and Discord live preview card.
#include "extra/ui/overlay/ac_overlay_rpc.h"

#include <cstring>
#include "imgui.h"
#include "extra/rpc/ac_discord_rpc.h"
#include "extra/rpc/ac_game_data.h"

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
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

ImVec4 GetStatusColor(bool enabled, bool connected) {
    if (!enabled) return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    if (connected) return ImVec4(0.24f, 0.90f, 0.40f, 1.0f);
    return ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
}

} // namespace

void RenderSection() {
    AcDiscordRpc::Init();
    auto& s = AcDiscordRpc::GetSettings();
    ImGui::SeparatorText("Discord Rich Presence (RPC)");

    const bool enabled = s.enabled.load(std::memory_order_relaxed);
    const bool connected = AcDiscordRpc::IsConnected();
    ImGui::TextColored(GetStatusColor(enabled, connected), "%s", AcDiscordRpc::GetStatusText());

    CheckboxAtomic("Ativar Discord Rich Presence", s.enabled);
    Hint("Exibe no seu perfil do Discord o capítulo (File), caso/missão, área atual e combate a bosses.");

    ImGui::BeginDisabled(!enabled);
    const bool win_open = s.window_open.load(std::memory_order_relaxed);
    const char* btn_text = win_open ? "Fechar Configuração do Discord###btn_ac_drpc_win"
                                    : "Configurar Presença do Discord###btn_ac_drpc_win";

    ImGui::PushStyleColor(ImGuiCol_Button, win_open ? ImVec4(0.20f, 0.45f, 0.40f, 1.0f) : ImVec4(0.24f, 0.36f, 0.68f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, win_open ? ImVec4(0.25f, 0.55f, 0.48f, 1.0f) : ImVec4(0.32f, 0.46f, 0.82f, 1.0f));
    if (ImGui::Button(btn_text, ImVec2(-32.0f, 0))) {
        s.window_open.store(!win_open, std::memory_order_relaxed);
    }
    ImGui::PopStyleColor(2);
    Hint("Abre janela com opções granulares (Capítulo, Missão, Dificuldade, Boss, Área, Tempo) e prévia em tempo real.");
    ImGui::EndDisabled();
}

void RenderWindow() {
    auto& s = AcDiscordRpc::GetSettings();
    if (!s.window_open.load(std::memory_order_relaxed)) return;

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Configuração do Discord Rich Presence###ac_discord_rpc_win", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) s.window_open.store(false, std::memory_order_relaxed);
        return;
    }
    if (!open) {
        s.window_open.store(false, std::memory_order_relaxed);
        ImGui::End();
        return;
    }

    const bool enabled = s.enabled.load(std::memory_order_relaxed);
    const bool connected = AcDiscordRpc::IsConnected();
    ImGui::TextColored(GetStatusColor(enabled, connected), "%s", AcDiscordRpc::GetStatusText());
    ImGui::SameLine(0.0f, 20.0f);
    CheckboxAtomic("Ativar Presença", s.enabled);
    ImGui::Separator();

    ImGui::BeginDisabled(!enabled);

    // Section 1: Agente & Missão
    ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "Agente & Missão");
    CheckboxAtomic("Exibir Arquivo / Capítulo (File 01..12)", s.show_file);
    Hint("Exibe o número e título do capítulo atual (ex: 'File 03: Link').");

    CheckboxAtomic("Exibir Missão / Caso Atual", s.show_mission);
    Hint("Exibe o nome do caso ou missão atual (ex: 'Maison Forest Search').");

    CheckboxAtomic("Exibir Dificuldade", s.show_difficulty);
    Hint("Exibe a dificuldade selecionada (Standard, Casual, Hardcore...).");

    ImGui::Spacing();

    // Section 2: Mundo & Combate
    ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "Mundo & Combate");
    CheckboxAtomic("Exibir Área / Localização Atual", s.show_area);
    Hint("Exibe a área atual (ex: 'Zone 09', 'Sector V', 'Astral Plane').");

    CheckboxAtomic("Exibir Combate contra Boss", s.show_boss);
    Hint("Mostra 'Lutando: <Nome do Boss>' na linha de estado quando em combate de boss.");

    ImGui::Spacing();

    // Section 3: Sessão & Tempo
    ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "Sessão & Tempo");
    CheckboxAtomic("Exibir Tempo de Sessão Decorrido", s.show_time);
    Hint("Mostra o cronômetro com tempo decorrido desde o início da sessão.");

    ImGui::Spacing();

    // Section 4: Aplicação Discord
    ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "Aplicação Discord (Client ID)");
    char cid_buf[64];
    std::strncpy(cid_buf, s.client_id.c_str(), sizeof(cid_buf) - 1);
    cid_buf[sizeof(cid_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::InputText("##ac_discord_cid", cid_buf, sizeof(cid_buf))) {
        s.client_id = cid_buf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Padrão (Astral Chain)")) {
        s.client_id = "1346000000000000000";
    }
    Hint("ID da aplicação Discord. Permite apontar para sua própria aplicação de Rich Presence no Developer Portal.");

    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();

    // Section 5: Live Discord Profile Preview Card!
    ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "Prévia em Tempo Real no Discord");

    auto act = AcDiscordRpc::GetCurrentActivity();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.15f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

    if (ImGui::BeginChild("##ac_discord_preview_card", ImVec2(0, 110.0f), ImGuiWindowFlags_NoScrollbar)) {
        ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.75f, 1.0f), "JOGANDO UM JOGO");

        ImVec2 icon_pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(icon_pos, ImVec2(icon_pos.x + 50.0f, icon_pos.y + 50.0f), IM_COL32(35, 38, 44, 255), 8.0f);
        dl->AddRect(icon_pos, ImVec2(icon_pos.x + 50.0f, icon_pos.y + 50.0f), IM_COL32(61, 209, 184, 230), 8.0f, 0, 1.5f);
        dl->AddText(ImVec2(icon_pos.x + 14.0f, icon_pos.y + 16.0f), IM_COL32(235, 237, 239, 255), "AC");

        ImGui::Dummy(ImVec2(58.0f, 52.0f));
        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Astral Chain");

        if (!act.details.empty()) {
            ImGui::TextColored(ImVec4(0.88f, 0.90f, 0.92f, 1.0f), "%s", act.details.c_str());
        }

        if (!act.state.empty()) {
            ImGui::TextColored(ImVec4(0.72f, 0.74f, 0.78f, 1.0f), "%s", act.state.c_str());
        }

        if (act.has_time && !act.time_text.empty()) {
            ImGui::TextColored(ImVec4(0.60f, 0.62f, 0.66f, 1.0f), "%s", act.time_text.c_str());
        }
        ImGui::EndGroup();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
}

} // namespace AcOverlayRpc
