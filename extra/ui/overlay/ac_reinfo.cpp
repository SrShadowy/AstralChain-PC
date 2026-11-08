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

#include "extra/ui/overlay/ac_reinfo.h"
#include "extra/ui/overlay/ac_reinfo_tables.h"

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
    if (!guest_range(a, n)) return false;
    memcpy(out, reinterpret_cast<const void *>(static_cast<uintptr_t>(a)), n);
    return true;
}

unsigned long long base_addr(void) { return ac_reinfo_main_base(); }

// Value behind a global slot: slot word -> pointer -> payload.
bool slot64(unsigned off, unsigned long long *out) {
    unsigned long long ptr = 0;
    const unsigned long long base = base_addr();
    if (!base || !read_raw(base + off, &ptr, sizeof ptr)) return false;
    if (!guest_range(ptr, 8) || (ptr & 3)) return false;
    *out = ptr;
    return true;
}

bool slot_u32(unsigned off, unsigned *out) {
    unsigned long long ptr = 0;
    return slot64(off, &ptr) && read_raw(ptr, out, sizeof *out);
}

bool slot_f32(unsigned off, float *out) {
    unsigned long long ptr = 0;
    return slot64(off, &ptr) && read_raw(ptr, out, sizeof *out);
}

bool field_u32(unsigned long long p, unsigned off, unsigned *out) {
    return p && read_raw(p + off, out, sizeof *out);
}

// A pointer value that would be a guest address, for the raw slot dump.
bool slot_word(unsigned off, unsigned long long *out) {
    const unsigned long long base = base_addr();
    return base && read_raw(base + off, out, sizeof *out);
}

// ---- name tables --------------------------------------------------------

const char *lookup_name(const AcNameEntry *t, int n, unsigned id) {
    for (int i = 0; i < n; i++)
        if (t[i].id == id) return t[i].name;
    return nullptr;
}

void name_cell(const AcNameEntry *t, int n, unsigned id) {
    const char *nm = lookup_name(t, n, id);
    if (!nm) ImGui::TextDisabled("(sem nome)");
    else if (!strcmp(nm, "-----")) ImGui::TextDisabled("(oculto)");
    else ImGui::TextUnformatted(nm);
}

// Full id -> name table, filtered by a per-window search box, current id marked.
// `filter` must outlive the frame: ImGui keeps editing the same buffer.
void table_section(const char *label, const AcNameEntry *t, int n, unsigned cur, bool has_cur,
                   char *filter, size_t filter_cap) {
    if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_None)) return;
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
            if (filter[0] && !strstr(id, filter) && !strstr(nm, filter)) continue;
            const bool hot = has_cur && t[i].id == cur;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (hot) ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "%s", id);
            else ImGui::TextUnformatted(id);
            ImGui::TableSetColumnIndex(1);
            if (hot) ImGui::TextColored(ImVec4(0.24f, 0.82f, 0.72f, 1.0f), "%s", nm);
            else ImGui::TextUnformatted(nm);
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
    if (!ok) { ImGui::TextDisabled("(n/d)"); return; }
    ImGui::Text("0x%04x", id);
    ImGui::TableSetColumnIndex(2);
    name_cell(t, n, id);
}

// Difficulty labels from the game's own jump table (0xbe08fc, used by the
// getter at 0x4c26e0): index = value + 2, valid for -2..2.
const char *difficulty_name(int raw) {
    static const char *k_names[] = {"super_casual", "error", "casual", "standard", "hardcore"};
    const int i = raw + 2;
    return (i >= 0 && i < AC_COUNT(k_names)) ? k_names[i] : nullptr;
}

void section_live(void) {
    ImGui::SeparatorText("Estado atual (leitura ao vivo)");
    if (!base_addr()) {
        ImGui::TextDisabled("main fora da janela guest (build sem a imagem carregada).");
        return;
    }
    unsigned area = 0, sub = 0, mission = 0, boss = 0, active = 0, stage = 0, battle = 0;
    float timer = 0.0f;
    const bool ok_area = slot_u32(0xdbcf10, &area);
    const bool ok_sub = slot_u32(0xdbcf20, &sub);
    const bool ok_mis = slot_u32(0xdbc420, &mission);
    const bool ok_boss = slot_u32(0xdbc950, &boss);
    const bool ok_act = slot_u32(0xdbc928, &active);
    const bool ok_stg = slot_u32(0xdbc930, &stage);
    const bool ok_btl = slot_u32(0xdbc940, &battle);
    const bool ok_tmr = slot_f32(0xdbc948, &timer);

    unsigned long long save = 0, scene = 0;
    const bool ok_save = slot64(0xdb83d8, &save);
    const bool ok_scene = slot64(0xdb84c8, &scene);
    unsigned long long playtime = 0;
    unsigned diff_raw = 0, diff_flags = 0, scene_id = 0, scene_phase = 0;
    const bool ok_play = ok_save && read_raw(save, &playtime, sizeof playtime);
    const bool ok_diff = ok_save && field_u32(save, 0xaefa0, &diff_raw);
    const bool ok_flags = ok_save && field_u32(save, 0xa8b8c, &diff_flags);
    const bool ok_sid = ok_scene && field_u32(scene, 0x180, &scene_id);
    const bool ok_phase = ok_scene && field_u32(scene, 0x100, &scene_phase);

    int file_num = 0;
    if (ok_mis && mission) {
        unsigned hi = (mission >> 8) & 0xFF;
        if (hi >= 0x21 && hi <= 0x2c) file_num = static_cast<int>(hi - 0x20);
        else if (hi >= 0x31 && hi <= 0x3b) file_num = static_cast<int>(hi - 0x30);
    }
    if (!file_num && ok_sid && scene_id >= 0x1100 && scene_id <= 0x1cff) {
        file_num = static_cast<int>(((scene_id >> 8) & 0xFF) - 0x10);
    }

    unsigned disp_area = (ok_area && area != 0xFFFFFFFF) ? area : 0;
    const char *scene_case = nullptr;
    if (ok_sid) {
        for (const auto &sc : k_ac_scenes) {
            if (sc.scene_id == scene_id) {
                if (!disp_area) disp_area = sc.area_id;
                scene_case = sc.case_name;
                break;
            }
        }
    }

    if (ImGui::BeginTable("live", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Campo");
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Nome / valor");
        ImGui::TableHeadersRow();
        name_row("File / Capitulo", file_num > 0, file_num, k_ac_files, AC_COUNT(k_ac_files));
        name_row("Area", disp_area != 0, disp_area, k_ac_areas, AC_COUNT(k_ac_areas));
        name_row("Missao", ok_mis, mission, k_ac_missions, AC_COUNT(k_ac_missions));
        name_row("Boss", ok_boss, boss, k_ac_bosses, AC_COUNT(k_ac_bosses));

        auto plain = [](const char *label, bool ok, const char *fmt, auto value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("--");
            ImGui::TableSetColumnIndex(2);
            if (ok) ImGui::Text(fmt, value);
            else ImGui::TextDisabled("(n/d)");
        };
        plain("Sub-area", ok_sub, "%u", sub);
        plain("Area >> 8 (chave do mapa)", ok_area, "0x%02x", area >> 8);
        plain("Boss ativo", ok_act, "%u", active);
        plain("Fase do boss", ok_stg, "%u", stage);
        plain("Estado de batalha", ok_btl, "%u", battle);
        plain("Timer de batalha", ok_tmr, "%.2f s", timer);
        plain("Playtime (seg)", ok_play, "%llu", playtime);
        plain("Dificuldade (raw +0xaefa0)", ok_diff, "%d", static_cast<int>(diff_raw));
        if (scene_case) {
            char sbuf[64];
            snprintf(sbuf, sizeof sbuf, "0x%04x (%s)", scene_id, scene_case);
            plain("Scene id (+0x180)", ok_sid, "%s", sbuf);
        } else {
            plain("Scene id (+0x180)", ok_sid, "0x%04x", scene_id);
        }
        plain("Scene phase (+0x100)", ok_phase, "0x%04x", scene_phase);
        ImGui::EndTable();
    }
    if (ok_play) {
        const unsigned long long h = playtime / 3600, m = (playtime / 60) % 60, s = playtime % 60;
        ImGui::Text("Playtime formatado: %03llu:%02llu:%02llu", h, m, s);
    }
    if (ok_diff) {
        const char *name = difficulty_name(static_cast<int>(diff_raw));
        if (name) ImGui::Text("Dificuldade: %s", name);
        else ImGui::TextDisabled("Dificuldade: (fora da faixa -2..2)");
    }
}

void section_slots(void) {
    static const struct { unsigned off; const char *label; const char *kind; } slots[] = {
        {0xdbcf10, "area", "u32"},        {0xdbcf20, "sub-area", "u32"},
        {0xdbc420, "mission", "u32"},     {0xdbc950, "boss id", "u32"},
        {0xdbc928, "boss ativo", "u32"},  {0xdbc930, "fase do boss", "u32"},
        {0xdbc940, "estado batalha", "u32"}, {0xdbc948, "timer batalha", "f32"},
        {0xdb83d8, "save struct", "ptr"}, {0xdb84c8, "scene struct", "ptr"},
    };
    if (!ImGui::CollapsingHeader("Slots globais (base + offset)")) return;
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
            if (ok) ImGui::Text("0x%010llx", word);
            else ImGui::TextDisabled("-");
        }
        ImGui::EndTable();
    }
}

void section_getters(void) {
    static const struct { unsigned off; const char *label; } getters[] = {
        {0x56c240, "area()"},          {0x56c250, "area() >> 8 (mapa)"},
        {0x56c270, "sub_area()"},      {0x4be600, "mission()"},
        {0x5407f0, "boss ativo?"},     {0x3e43a0, "segundos -> h/m/s"},
        {0x4c26e0, "label de dificuldade"}, {0x588080, "chave de boss"},
        {0x578c10, "chave de area"},   {0x587d30, "lookup de texto"},
        {0x587dc0, "lookup por chave"}, {0x91e170, "hash de chave"},
    };
    if (!ImGui::CollapsingHeader("Getters (base + offset)")) return;
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
    static const struct { unsigned off; const char *fmt; const char *use; } keys[] = {
        {0xb6327d, "MENU_DATA_AREA_NAME_%04x", "area (categoria 5, menu_text)"},
        {0xb76832, "MENU_MISSION_NAME_%04x", "missao (categoria 5, menu_text)"},
        {0xb74339, "EVENT_BOSS_NAME_%04x", "boss (categoria 8, GameWord)"},
        {0xb61e7c, "CORE_MAP_AREA_%04x", "mapa (Core_text)"},
        {0xb62fb7, "%03d:%02d:%02d", "playtime"},
    };
    if (!ImGui::CollapsingHeader("Chaves de texto (format strings)")) return;
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
    for (int i = 0; i < AC_COUNT(diff); i++) ImGui::BulletText("%+d  %s", i - 2, diff[i]);
}

bool g_visible;  // closed by default: F8 or AC_REINFO=1 opens it
bool g_boot_done;

void boot_state(void) {
    if (g_boot_done) return;
    g_boot_done = true;
    const char *e = getenv("AC_REINFO");
    if (e && atoi(e) != 0) g_visible = true;
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
    if (!g_visible) return;
    unsigned area = 0, mission = 0, boss = 0;
    const bool ok_area = slot_u32(0xdbcf10, &area);
    const bool ok_mis = slot_u32(0xdbc420, &mission);
    const bool ok_boss = slot_u32(0xdbc950, &boss);

    ImGui::SetNextWindowSize(ImVec2(600, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(520, 40), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("RE Info (Astral Chain)", &g_visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    section_live();
    section_slots();
    section_getters();
    section_text_keys();
    // One persistent filter buffer per table (ImGui edits it across frames).
    static char f_areas[32], f_missions[32], f_bosses[32];
    ImGui::SeparatorText("Tabelas id -> nome");
    table_section("Areas (22)", k_ac_areas, AC_COUNT(k_ac_areas), area, ok_area,
                  f_areas, sizeof f_areas);
    table_section("Missoes (292)", k_ac_missions, AC_COUNT(k_ac_missions), mission, ok_mis,
                  f_missions, sizeof f_missions);
    table_section("Bosses (82)", k_ac_bosses, AC_COUNT(k_ac_bosses), boss, ok_boss,
                  f_bosses, sizeof f_bosses);
    ImGui::End();
}
