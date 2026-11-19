#include "ac_menu_tab_sdk.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "imgui.h"
#include "additions/overlay/ac_hud.h"
#include "additions/sdk/ac_sdk.hpp"

namespace {

struct SdkItem {
    const char* module_name;
    const char* func_name;
    const char* ret_type;
    std::string (*getter)();
    std::string cached_value;
};

std::string fmt_bool(bool b) {
    return b ? "true" : "false";
}

std::string fmt_float(float f) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", f);
    return buf;
}

std::string fmt_int(int32_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    return buf;
}

std::string fmt_uint(uint32_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u (0x%x)", val, val);
    return buf;
}

std::string fmt_hex(uintptr_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(val));
    return buf;
}

std::string fmt_vec3(const ac::Vec3& v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
    return buf;
}

std::string fmt_stick(const ac::StickVector& v) {
    char buf[48];
    snprintf(buf, sizeof(buf), "(%.2f, %.2f)", v.x, v.y);
    return buf;
}

const char* legion_name(ac::game_state::LegionType t) {
    switch (t) {
        case ac::game_state::LegionType::Sword: return "Sword";
        case ac::game_state::LegionType::Arrow: return "Arrow";
        case ac::game_state::LegionType::Arm:   return "Arm";
        case ac::game_state::LegionType::Beast: return "Beast";
        case ac::game_state::LegionType::Axe:   return "Axe";
        default:                                return "None (-1)";
    }
}

const char* voice_name(ac::game_state::VoiceLanguage l) {
    switch (l) {
        case ac::game_state::VoiceLanguage::Japanese: return "Japanese (0)";
        case ac::game_state::VoiceLanguage::English:  return "English (1)";
        default:                                      return "Auto (-1)";
    }
}

const char* text_lang_name(ac::game_state::TextLanguage l) {
    switch (l) {
        case ac::game_state::TextLanguage::Japanese:             return "Japanese (0)";
        case ac::game_state::TextLanguage::AmericanEnglish:      return "English US (1)";
        case ac::game_state::TextLanguage::French:              return "French (2)";
        case ac::game_state::TextLanguage::German:              return "German (3)";
        case ac::game_state::TextLanguage::Italian:             return "Italian (4)";
        case ac::game_state::TextLanguage::Spanish:             return "Spanish (5)";
        case ac::game_state::TextLanguage::SimplifiedChinese:   return "Simplified Chinese (6)";
        case ac::game_state::TextLanguage::TraditionalChinese:  return "Traditional Chinese (7)";
        case ac::game_state::TextLanguage::Korean:              return "Korean (8)";
        case ac::game_state::TextLanguage::Dutch:               return "Dutch (9)";
        case ac::game_state::TextLanguage::Portuguese:          return "Portuguese (10)";
        case ac::game_state::TextLanguage::Russian:             return "Russian (11)";
        case ac::game_state::TextLanguage::Taiwanese:           return "Taiwanese (12)";
        case ac::game_state::TextLanguage::BritishEnglish:      return "British English (13)";
        case ac::game_state::TextLanguage::LatinAmericanSpanish: return "Latin American Spanish (14)";
        default:                                                return "Auto (-1)";
    }
}

static SdkItem s_items[] = {
    // ac::camera
    {"camera", "ac::camera::get_camera_manager()", "uintptr_t", []() -> std::string {
        uintptr_t mgr = ac::camera::get_camera_manager();
        return mgr ? fmt_hex(mgr) : "0x0 (Inativo)";
    }, "-"},
    {"camera", "ac::camera::get_active_camera()", "uintptr_t", []() -> std::string {
        uintptr_t cam = ac::camera::get_active_camera();
        return cam ? fmt_hex(cam) : "0x0 (Inativa)";
    }, "-"},
    {"camera", "ac::camera::get_fov(0)", "float", []() -> std::string { return fmt_float(ac::camera::get_fov(0)) + " deg"; }, "-"},
    {"camera", "ac::camera::is_free_camera_enabled(0)", "bool", []() -> std::string { return fmt_bool(ac::camera::is_free_camera_enabled(0)); }, "-"},
    {"camera", "ac::camera::get_camera_shake_factor(0)", "float", []() -> std::string { return fmt_float(ac::camera::get_camera_shake_factor(0)); }, "-"},

    // ac::time
    {"time", "ac::time::get_frame_delta_ms()", "float", []() -> std::string { return fmt_float(ac::time::get_frame_delta_ms()) + " ms"; }, "-"},
    {"time", "ac::time::get_target_fps()", "float", []() -> std::string { return fmt_float(ac::time::get_target_fps()) + " FPS"; }, "-"},
    {"time", "ac::time::get_target_rate()", "float", []() -> std::string { return fmt_float(ac::time::get_target_rate()); }, "-"},
    {"time", "ac::time::get_game_time_scale()", "float", []() -> std::string { return fmt_float(ac::time::get_game_time_scale()); }, "-"},
    {"time", "ac::time::get_combat_delta_scale(60.0f)", "float", []() -> std::string { return fmt_float(ac::time::get_combat_delta_scale(60.0f)); }, "-"},

    // ac::ui
    {"ui", "ac::ui::is_menu_open()", "bool", []() -> std::string { return fmt_bool(ac::ui::is_menu_open()); }, "-"},
    {"ui", "ac::ui::get_active_menu()", "int32_t", []() -> std::string { return fmt_int(ac::ui::get_active_menu()); }, "-"},
    {"ui", "ac::ui::is_in_gameplay()", "bool", []() -> std::string { return fmt_bool(ac::ui::is_in_gameplay()); }, "-"},
    {"ui", "ac::ui::is_gameplay_paused()", "bool", []() -> std::string { return fmt_bool(ac::ui::is_gameplay_paused()); }, "-"},
    {"ui", "ac::ui::is_camera_frozen()", "bool", []() -> std::string { return fmt_bool(ac::ui::is_camera_frozen()); }, "-"},
    {"ui", "ac::ui::is_engine_transitioning()", "bool", []() -> std::string { return fmt_bool(ac::ui::is_engine_transitioning()); }, "-"},

    // ac::game_state
    {"game_state", "ac::game_state::get_scene_id()", "uint32_t", []() -> std::string { return fmt_uint(ac::game_state::get_scene_id()); }, "-"},
    {"game_state", "ac::game_state::get_phase_id()", "uint32_t", []() -> std::string { return fmt_uint(ac::game_state::get_phase_id()); }, "-"},
    {"game_state", "ac::game_state::get_area_id()", "uint32_t", []() -> std::string { return fmt_uint(ac::game_state::get_area_id()); }, "-"},
    {"game_state", "ac::game_state::get_subarea_id()", "uint32_t", []() -> std::string { return fmt_uint(ac::game_state::get_subarea_id()); }, "-"},
    {"game_state", "ac::game_state::is_boss_active()", "bool", []() -> std::string { return fmt_bool(ac::game_state::is_boss_active()); }, "-"},
    {"game_state", "ac::game_state::get_boss_id()", "uint32_t", []() -> std::string { return fmt_uint(ac::game_state::get_boss_id()); }, "-"},
    {"game_state", "ac::game_state::get_active_legion()", "LegionType", []() -> std::string { return std::string(legion_name(ac::game_state::get_active_legion())); }, "-"},
    {"game_state", "ac::game_state::get_voice_language()", "VoiceLanguage", []() -> std::string { return std::string(voice_name(ac::game_state::get_voice_language())); }, "-"},
    {"game_state", "ac::game_state::get_text_language()", "TextLanguage", []() -> std::string { return std::string(text_lang_name(ac::game_state::get_text_language())); }, "-"},
    {"game_state", "ac::game_state::get_player_actor(0)", "uintptr_t", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        return actor ? fmt_hex(actor) : "0x0 (Sem ator)";
    }, "-"},
    {"game_state", "ac::game_state::get_player_hp(0)", "int32_t", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "0 (Sem ator)";
        return fmt_int(ac::game_state::get_player_hp(0));
    }, "-"},
    {"game_state", "ac::game_state::get_player_max_hp(0)", "int32_t", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "1000 (Padrao / Sem ator)";
        return fmt_int(ac::game_state::get_player_max_hp(0));
    }, "-"},
    {"game_state", "ac::game_state::get_legion_energy(0)", "float", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "0.000 % (Sem ator)";
        return fmt_float(ac::game_state::get_legion_energy(0)) + " %";
    }, "-"},
    {"game_state", "ac::game_state::get_legion_max_energy(0)", "float", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "100.000 (Sem ator)";
        return fmt_float(ac::game_state::get_legion_max_energy(0));
    }, "-"},
    {"game_state", "ac::game_state::get_legion_energy_raw(0)", "float", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "0.000 (Sem ator)";
        return fmt_float(ac::game_state::get_legion_energy_raw(0));
    }, "-"},
    {"game_state", "ac::game_state::get_player_position(0)", "Vec3", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "(0.00, 0.00, 0.00) (Inativo)";
        return fmt_vec3(ac::game_state::get_player_position(0));
    }, "-"},
    {"game_state", "ac::game_state::get_player_rotation(0)", "Vec3", []() -> std::string {
        uintptr_t actor = ac::game_state::get_player_actor(0);
        if (!actor) return "(0.00, 0.00, 0.00) (Inativo)";
        return fmt_vec3(ac::game_state::get_player_rotation(0));
    }, "-"},

    // ac::input
    {"input", "ac::input::get_buttons_held()", "uint32_t", []() -> std::string { return fmt_hex(ac::input::get_buttons_held()); }, "-"},
    {"input", "ac::input::get_right_stick()", "StickVector", []() -> std::string { return fmt_stick(ac::input::get_right_stick()); }, "-"},
    {"input", "ac::input::get_left_stick()", "StickVector", []() -> std::string { return fmt_stick(ac::input::get_left_stick()); }, "-"},

    // ac::sound
    {"sound", "ac::sound::get_master_volume()", "int32_t", []() -> std::string { return fmt_int(ac::sound::get_master_volume()) + " %"; }, "-"},
    {"sound", "ac::sound::get_bgm_volume()", "int32_t", []() -> std::string { return fmt_int(ac::sound::get_bgm_volume()) + " %"; }, "-"},
    {"sound", "ac::sound::is_mute_on_unfocus()", "bool", []() -> std::string { return fmt_bool(ac::sound::is_mute_on_unfocus()); }, "-"},

    // ac::mods
    {"mods", "ac::mods::get_active_mod_count()", "size_t", []() -> std::string { return std::to_string(ac::mods::get_active_mod_count()); }, "-"},
    {"mods", "ac::mods::list_mods().size()", "size_t", []() -> std::string { return std::to_string(ac::mods::list_mods().size()) + " total"; }, "-"},
    {"mods", "ac::mods::has_active_translation_mod()", "bool", []() -> std::string { return fmt_bool(ac::mods::has_active_translation_mod()); }, "-"},

    // ac::memory
    {"memory", "ac::memory::get_main_base()", "uintptr_t", []() -> std::string { return fmt_hex(ac::memory::get_main_base()); }, "-"}
};

constexpr size_t kItemCount = sizeof(s_items) / sizeof(s_items[0]);

bool match_filter(const SdkItem& item, const char* query) {
    if (!query || !*query) return true;
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string mod = item.module_name;
    std::string fn = item.func_name;
    std::transform(mod.begin(), mod.end(), mod.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(fn.begin(), fn.end(), fn.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return mod.find(q) != std::string::npos || fn.find(q) != std::string::npos;
}

} // namespace

void ac_menu_tab_sdk(void) {
    ac_ui_section_header("MINI-SDK // EXPLORER & LEITURA EM TEMPO REAL", "// SDK.DEV");

    static char s_search[64] = "";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 170.0f);
    ImGui::InputTextWithHint("##sdk_search", "Buscar funcao (ex: hp, camera, time, mod)...", s_search, sizeof(s_search));

    ImGui::SameLine();
    if (ImGui::Button("Ler Visiveis", ImVec2(90.0f, 0.0f))) {
        for (size_t i = 0; i < kItemCount; i++) {
            if (match_filter(s_items[i], s_search)) {
                s_items[i].cached_value = s_items[i].getter();
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Limpar", ImVec2(65.0f, 0.0f))) {
        s_search[0] = '\0';
    }

    ImGui::Spacing();

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg |
                                            ImGuiTableFlags_BordersInnerV |
                                            ImGuiTableFlags_SizingStretchProp |
                                            ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##sdk_functions_table", 4, table_flags, ImVec2(0, 360.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Funcao SDK", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("Tipo", ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Valor Retornado", ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn("Acao", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < kItemCount; i++) {
            if (!match_filter(s_items[i], s_search)) continue;

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(AcIrisColors::Cyan, "%s", s_items[i].func_name);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", s_items[i].ret_type);

            ImGui::TableSetColumnIndex(2);
            if (s_items[i].cached_value == "-") {
                ImGui::TextDisabled("-");
            } else {
                ImGui::TextColored(AcIrisColors::TextMain, "%s", s_items[i].cached_value.c_str());
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("Ler")) {
                s_items[i].cached_value = s_items[i].getter();
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Dica: Clique em [Ler] ao lado de qualquer funcao para inspecionar o retorno real da engine.");
}
