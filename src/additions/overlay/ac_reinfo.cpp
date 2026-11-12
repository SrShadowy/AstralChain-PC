// [extra] Read-only "RE Info" window (Insert opens the overlay): every gameplay
// value recovered by static RE, live from guest memory, plus the address map,
// the getter list and the id->name tables extracted from the game text archives.
// Nothing here writes to guest memory; every read is range-checked against the
// guest window first, so a null/stale slot simply shows as unavailable.
//
// Address notes (main module, base = 0x7000010000):
// - "slot" offsets live in .data/.bss and hold a pointer to the real storage, so
//   a value costs two dereferences (slot -> block -> value). See the RE doc.
// - Runtime address = 0x7000010000 + documented offset. The linker lays out
//   rtld (0x4000) then main then sdk starting at 0x7000000000, so main is the
//   second module, not the first (ac_reinfo_probe.c derives the constant).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"

#include "additions/overlay/ac_hud.h"
#include "additions/overlay/ac_reinfo.h"
#include "additions/overlay/ac_reinfo_tables.h"
#include "additions/rpc/ac_game_data.h"

extern "C" unsigned long long ac_reinfo_main_base(void);   // (ac_reinfo_probe.c)
extern "C" unsigned long long ac_reinfo_guest_lo(void);
extern "C" unsigned long long ac_reinfo_guest_hi(void);

#define AC_COUNT(a) (static_cast<int>(sizeof(a) / sizeof((a)[0])))

namespace {

// ---- guest memory (read only) -------------------------------------------

bool guest_range(unsigned long long a, size_t n) {
    const unsigned long long lo = ac_reinfo_guest_lo();
    const unsigned long long hi = ac_reinfo_guest_hi();
    return a >= lo && n > 0 && a + n <= hi;  // guest window is one mapping: reads never fault
}

bool read_raw(unsigned long long a, void *out, size_t n) {
    if (!guest_range(a, n)) {
        return false;
    }
    memcpy(out, reinterpret_cast<const void *>(static_cast<uintptr_t>(a)), n);
    return true;
}

unsigned long long base_addr(void) {
    return ac_reinfo_main_base();
}

// A pointer value that would be a guest address, for the raw slot dump.
bool slot_word(unsigned off, unsigned long long *out) {
    const unsigned long long base = base_addr();
    return base && read_raw(base + off, out, sizeof *out);
}

// ---- name tables --------------------------------------------------------

const char *lookup_name(const AcNameEntry *t, int n, unsigned id) {
    for (int i = 0; i < n; i++) {
        if (t[i].id == id) {
            return t[i].name;
        }
    }
    return nullptr;
}

void name_cell(const AcNameEntry *t, int n, unsigned id) {
    const char *nm = lookup_name(t, n, id);
    if (!nm) {
        ImGui::TextDisabled("(sem nome)");
    } else if (!strcmp(nm, "-----")) {
        ImGui::TextDisabled("(oculto)");
    } else {
        ImGui::TextUnformatted(nm);
    }
}

// Full id -> name table, filtered by a per-window search box, current id marked.
// `filter` must outlive the frame: ImGui keeps editing the same buffer.
void table_section(const char *label, const AcNameEntry *t, int n, unsigned cur, bool has_cur,
                   char *filter, size_t filter_cap) {
    if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_None)) {
        return;
    }
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "filtrar id ou nome", filter, filter_cap);
    if (ImGui::BeginTable(label, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                                      ImGuiTableFlags_ScrollY, ImVec2(0, 160))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Nome");
        ImGui::TableHeadersRow();
        for (int i = 0; i < n; i++) {
            char id[16];
            snprintf(id, sizeof id, "0x%04x", t[i].id);
            const char *nm = t[i].name ? t[i].name : "(sem nome)";
            if (filter[0] && !strstr(id, filter) && !strstr(nm, filter)) {
                continue;
            }
            const bool hot = has_cur && t[i].id == cur;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (hot) {
                ImGui::TextColored(AcIrisColors::Cyan, "%s", id);
            } else {
                ImGui::TextUnformatted(id);
            }
            ImGui::TableSetColumnIndex(1);
            if (hot) {
                ImGui::TextColored(AcIrisColors::Cyan, "%s", nm);
            } else {
                ImGui::TextUnformatted(nm);
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

// ---- sections ----------------------------------------------------------

void name_row(const char *label, bool ok, unsigned id, const AcNameEntry *t, int n) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    if (!ok) {
        ImGui::TextDisabled("(n/d)");
        return;
    }
    ImGui::Text("0x%04x", id);
    ImGui::TableSetColumnIndex(2);
    name_cell(t, n, id);
}

void section_live(const AcGameData::PresenceSnapshot &s) {
    ac_ui_section_header("ESTADO ATUAL (LEITURA AO VIVO)", "// MEM.LIVE");
    if (!base_addr()) {
        ImGui::TextDisabled("main fora da janela guest (build sem a imagem carregada).");
        return;
    }
    if (ImGui::BeginTable("live", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Campo");
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Nome / valor");
        ImGui::TableHeadersRow();
        name_row("File / Capitulo", s.file_num > 0, static_cast<unsigned>(s.file_num), k_ac_files, AC_COUNT(k_ac_files));
        name_row("Area (canonica)", s.area_id != 0, s.area_id, k_ac_areas, AC_COUNT(k_ac_areas));
        name_row("Objetivo (Caso)", s.purpose_id != 0, s.purpose_id, k_ac_purposes, AC_COUNT(k_ac_purposes));
        name_row("Missao", s.mission_id != 0, s.mission_id, k_ac_missions, AC_COUNT(k_ac_missions));
        name_row("Legiao ativa", s.legion_id >= 0, static_cast<unsigned>(s.legion_id), k_ac_legions, AC_COUNT(k_ac_legions));
        name_row("Boss", s.boss_id != 0, s.boss_id, k_ac_bosses, AC_COUNT(k_ac_bosses));

        auto plain = [](const char *label, bool ok, const char *fmt, auto value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("--");
            ImGui::TableSetColumnIndex(2);
            if (ok) {
                ImGui::Text(fmt, value);
            } else {
                ImGui::TextDisabled("(n/d)");
            }
        };
        plain("Trigger area (0xdbcf10)", s.raw_area_id != 0, "0x%04x", s.raw_area_id);
        plain("Sub-area", true, "%u", s.sub_area);
        plain("Phase id (0xdb84f8)", s.phase_id != 0, "0x%04x", s.phase_id);
        plain("Scene id (+0x180)", s.scene_id != 0, "0x%04x", s.scene_id);
        plain("Scenario step (0xdb8930)", s.scenario_step >= 0, "%d", s.scenario_step);
        plain("Boss ativo", true, "%u", s.boss_active ? 1u : 0u);
        plain("Fase do boss", true, "%u", s.boss_stage);
        plain("Estado de batalha", true, "%u", s.battle_state);
        plain("Timer de batalha", true, "%.2f s", s.battle_timer);
        plain("Playtime (seg)", s.total_playtime_s > 0, "%llu", static_cast<unsigned long long>(s.total_playtime_s));
        plain("Dificuldade", !s.difficulty_name.empty(), "%s", s.difficulty_name.c_str());
        ImGui::EndTable();
    }
    if (s.total_playtime_s > 0) {
        const unsigned long long h = s.total_playtime_s / 3600;
        const unsigned long long m = (s.total_playtime_s / 60) % 60;
        const unsigned long long sec = s.total_playtime_s % 60;
        ImGui::Text("Playtime formatado: %03llu:%02llu:%02llu", h, m, sec);
    }
}

struct ReSlotDesc {
    unsigned off;
    const char *label;
    const char *kind;
};

struct ReGetterDesc {
    unsigned off;
    const char *label;
};

struct ReKeyDesc {
    unsigned off;
    const char *fmt;
    const char *use;
};

void section_slots(void) {
    static const ReSlotDesc slots[] = {
        {AcGameData::kSlotArea,        "area trigger",   "u32"},
        {AcGameData::kSlotSubArea,     "sub-area",       "u32"},
        {AcGameData::kSlotMission,     "mission",        "u32"},
        {AcGameData::kSlotLegion,      "legion ativa",   "s32"},
        {AcGameData::kSlotBossId,      "boss id",        "u32"},
        {AcGameData::kSlotBossActive,  "boss ativo",     "u32"},
        {AcGameData::kSlotBossStage,   "fase do boss",   "u32"},
        {AcGameData::kSlotBattleState, "estado batalha", "u32"},
        {AcGameData::kSlotBattleTimer, "timer batalha",  "f32"},
        {AcGameData::kSlotSave,        "save struct",    "ptr"},
        {AcGameData::kSlotScene,       "scene struct",   "ptr"},
        {AcGameData::kSlotPhase,       "phase id",       "u32"},
        {AcGameData::kSlotScenario,    "scenario mgr",   "ptr"},
    };
    if (!ImGui::CollapsingHeader("Slots globais (base + offset)")) {
        return;
    }
    ImGui::TextDisabled("Valor = dupla desreferencia do slot; '-' quando fora do guest.");
    if (ImGui::BeginTable("slots", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Campo");
        ImGui::TableSetupColumn("Tipo");
        ImGui::TableSetupColumn("Runtime");
        ImGui::TableHeadersRow();
        for (const auto &s : slots) {
            unsigned long long word = 0;
            const bool ok = slot_word(s.off, &word);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("0x%06x", s.off);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(s.label);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", s.kind);
            ImGui::TableSetColumnIndex(3);
            if (ok) {
                ImGui::Text("0x%010llx", word);
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }
}

void section_getters(void) {
    static const ReGetterDesc getters[] = {
        {0x56c240, "area()"},
        {0x56c250, "area() >> 8 (mapa)"},
        {0x56c270, "sub_area()"},
        {0x4be600, "mission()"},
        {0x5407f0, "boss ativo?"},
        {0x3e43a0, "segundos -> h/m/s"},
        {0x4c26e0, "label de dificuldade"},
        {0x588080, "chave de boss"},
        {0x578c10, "chave de area"},
        {0x587d30, "lookup de texto"},
        {0x587dc0, "lookup por chave"},
        {0x91e170, "hash de chave"},
    };
    if (!ImGui::CollapsingHeader("Getters (base + offset)")) {
        return;
    }
    ImGui::TextDisabled("Endereco de entrada; chamar exige o ABI guest (nao invocado aqui).");
    if (ImGui::BeginTable("getters", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Getter");
        ImGui::TableHeadersRow();
        for (const auto &g : getters) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("0x%06x", g.off);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(g.label);
        }
        ImGui::EndTable();
    }
}

void section_text_keys(void) {
    static const ReKeyDesc keys[] = {
        {0xb6327d, "MENU_DATA_AREA_NAME_%04x", "area (categoria 5, menu_text)"},
        {0xb76832, "MENU_MISSION_NAME_%04x", "missao (categoria 5, menu_text)"},
        {0xb74339, "EVENT_BOSS_NAME_%04x", "boss (categoria 8, GameWord)"},
        {0xb61e7c, "CORE_MAP_AREA_%04x", "mapa (Core_text)"},
        {0xb62fb7, "%03d:%02d:%02d", "playtime"},
    };
    if (!ImGui::CollapsingHeader("Chaves de texto (format strings)")) {
        return;
    }
    if (ImGui::BeginTable("keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Offset");
        ImGui::TableSetupColumn("Formato");
        ImGui::TableSetupColumn("Uso");
        ImGui::TableHeadersRow();
        for (const auto &k : keys) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("0x%06x", k.off);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(k.fmt);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", k.use);
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Dificuldade (-2..2)");
    static const char *diff[] = {"super_casual", "error", "casual", "standard", "hardcore"};
    for (int i = 0; i < AC_COUNT(diff); i++) {
        ImGui::BulletText("%+d  %s", i - 2, diff[i]);
    }
}

bool g_visible;  // closed by default: F8 or AC_REINFO=1 opens it
bool g_boot_done;

void boot_state(void) {
    if (g_boot_done) {
        return;
    }
    g_boot_done = true;
    const char *e = getenv("AC_REINFO");
    if (e && atoi(e) != 0) {
        g_visible = true;
    }
}

}  // namespace

extern "C" int ac_reinfo_visible(void) {
    boot_state();
    return g_visible ? 1 : 0;
}

extern "C" void ac_reinfo_toggle(void) {
    boot_state();
    g_visible = !g_visible;
}

extern "C" void ac_reinfo_frame(void) {
    boot_state();
    if (!g_visible) {
        return;
    }
    const auto snap = AcGameData::GetSnapshot();

    ImGui::SetNextWindowSize(ImVec2(600, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(580, 36), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("IRIS // RE INFO (ASTRAL CHAIN)###ac_reinfo_win", &g_visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ac_ui_draw_window_chrome();
    section_live(snap);
    section_slots();
    section_getters();
    section_text_keys();
    static char f_areas[32], f_purposes[32], f_missions[32], f_bosses[32];
    ac_ui_section_header("TABELAS ID -> NOME", "// DB.TABLES");
    table_section("Áreas (22)", k_ac_areas, AC_COUNT(k_ac_areas), snap.area_id, snap.area_id != 0,
                  f_areas, sizeof f_areas);
    table_section("Objetivos (118)", k_ac_purposes, AC_COUNT(k_ac_purposes), snap.purpose_id, snap.purpose_id != 0,
                  f_purposes, sizeof f_purposes);
    table_section("Missões (292)", k_ac_missions, AC_COUNT(k_ac_missions), snap.mission_id, snap.mission_id != 0,
                  f_missions, sizeof f_missions);
    table_section("Bosses (82)", k_ac_bosses, AC_COUNT(k_ac_bosses), snap.boss_id, snap.boss_id != 0,
                  f_bosses, sizeof f_bosses);
    ImGui::End();
}
