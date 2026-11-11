// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Guest memory reader and game state aggregator for Astral Chain RPC.
#include "additions/rpc/ac_game_data.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#include "additions/overlay/ac_reinfo_tables.h"

extern "C" unsigned long long ac_reinfo_main_base(void);
extern "C" unsigned long long ac_reinfo_guest_lo(void);
extern "C" unsigned long long ac_reinfo_guest_hi(void);

namespace AcGameData {
namespace {

int64_t g_session_start_unix = 0;
auto g_session_steady_start = std::chrono::steady_clock::now();

bool guest_range(unsigned long long a, size_t n) {
    const unsigned long long lo = ac_reinfo_guest_lo();
    const unsigned long long hi = ac_reinfo_guest_hi();
    return a >= lo && n > 0 && a + n <= hi;
}

bool read_raw(unsigned long long a, void *out, size_t n) {
    if (!guest_range(a, n)) return false;
    std::memcpy(out, reinterpret_cast<const void *>(static_cast<uintptr_t>(a)), n);
    return true;
}

bool slot64(unsigned off, unsigned long long *out) {
    unsigned long long ptr = 0;
    const unsigned long long base = ac_reinfo_main_base();
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

const char* lookup_name(const AcNameEntry *t, size_t n, unsigned id) {
    for (size_t i = 0; i < n; ++i) {
        if (t[i].id == id) return t[i].name;
    }
    return nullptr;
}

const AcSceneEntry* lookup_scene(const AcSceneEntry *t, size_t n, unsigned scene_id) {
    for (size_t i = 0; i < n; ++i) {
        if (t[i].scene_id == scene_id) return &t[i];
    }
    return nullptr;
}

const char* difficulty_to_str(int raw) {
    static const char *k_diff[] = {"Super Casual", "Casual", "Standard", "Hardcore"};
    if (raw == -2) return k_diff[0];
    if (raw == 0) return k_diff[1];
    if (raw == 1) return k_diff[2];
    if (raw == 2) return k_diff[3];
    return "Standard";
}

int derive_file_num(unsigned mission_id) {
    if (!mission_id) return 0;
    unsigned hi = (mission_id >> 8) & 0xFF;
    if (hi >= 0x21 && hi <= 0x2c) return static_cast<int>(hi - 0x20);
    if (hi >= 0x31 && hi <= 0x3b) return static_cast<int>(hi - 0x30);
    return 0;
}

} // namespace

void Init() {
    if (g_session_start_unix == 0) {
        auto now = std::chrono::system_clock::now();
        g_session_start_unix = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        g_session_steady_start = std::chrono::steady_clock::now();
    }
}

uint64_t GetSessionElapsedSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - g_session_steady_start).count();
}

PresenceSnapshot GetSnapshot() {
    Init();
    PresenceSnapshot s;
    s.session_start_unix_s = g_session_start_unix;

    if (!ac_reinfo_main_base()) return s;

    (void)slot_u32(0xdbcf10, &s.area_id);
    (void)slot_u32(0xdbcf20, &s.sub_area);
    (void)slot_u32(0xdbc420, &s.mission_id);
    (void)slot_u32(0xdbc950, &s.boss_id);

    unsigned b_act = 0;
    if (slot_u32(0xdbc928, &b_act)) s.boss_active = (b_act == 1);
    (void)slot_u32(0xdbc930, &s.boss_stage);
    (void)slot_u32(0xdbc940, &s.battle_state);
    (void)slot_f32(0xdbc948, &s.battle_timer);

    unsigned long long scene_ptr = 0;
    if (slot64(0xdb84c8, &scene_ptr)) {
        (void)read_raw(scene_ptr + 0x180, &s.scene_id, sizeof s.scene_id);
    }

    unsigned long long save_ptr = 0;
    if (slot64(0xdb83d8, &save_ptr)) {
        (void)read_raw(save_ptr, &s.total_playtime_s, sizeof s.total_playtime_s);
        unsigned diff_raw = 0;
        if (read_raw(save_ptr + 0xaefa0, &diff_raw, sizeof diff_raw)) {
            s.difficulty_raw = static_cast<int>(diff_raw);
            s.difficulty_name = difficulty_to_str(s.difficulty_raw);
        }
    }

    s.in_game = (s.total_playtime_s > 0 || (s.scene_id >= 0x1100 && s.scene_id <= 0x1cff) ||
                 s.area_id != 0 || s.mission_id != 0);

    // Filter out uninitialized / outside-volume -1 (0xFFFFFFFF) from slot 0xdbcf10
    if (s.area_id == 0xFFFFFFFF || s.area_id >= 0x10000) {
        s.area_id = 0;
    }

    // Resolve Scene Info (Case name and canonical area fallback)
    const AcSceneEntry* sc = nullptr;
    if (s.scene_id != 0) {
        sc = lookup_scene(k_ac_scenes, sizeof(k_ac_scenes)/sizeof(k_ac_scenes[0]), s.scene_id);
        if (sc && sc->case_name) {
            s.case_name = sc->case_name;
        }
    }

    // Look up Area Name: use active trigger area or fall back to canonical scene area
    const char* aname = nullptr;
    if (s.area_id != 0) {
        aname = lookup_name(k_ac_areas, sizeof(k_ac_areas)/sizeof(k_ac_areas[0]), s.area_id);
    }
    if (!aname && sc && sc->area_id != 0) {
        s.area_id = sc->area_id;
        aname = lookup_name(k_ac_areas, sizeof(k_ac_areas)/sizeof(k_ac_areas[0]), s.area_id);
    }
    s.area_name = aname ? aname : "";

    // Look up Boss Name
    const char* bname = lookup_name(k_ac_bosses, sizeof(k_ac_bosses)/sizeof(k_ac_bosses[0]), s.boss_id);
    s.boss_name = bname ? bname : "";

    // Derive File / Chapter
    s.file_num = derive_file_num(s.mission_id);
    if (s.file_num == 0 && s.scene_id >= 0x1100 && s.scene_id <= 0x1cff) {
        s.file_num = static_cast<int>(((s.scene_id >> 8) & 0xFF) - 0x10);
    }
    if (s.file_num >= 1 && s.file_num <= 12) {
        const char* fname = lookup_name(k_ac_files, sizeof(k_ac_files)/sizeof(k_ac_files[0]), s.file_num);
        s.file_name = fname ? fname : "";
    }

    // Look up Mission / Case Name: active combat mission or current story Case
    const char* mname = nullptr;
    if (s.mission_id != 0) {
        mname = lookup_name(k_ac_missions, sizeof(k_ac_missions)/sizeof(k_ac_missions[0]), s.mission_id);
    }
    if (mname) {
        s.mission_name = mname;
    } else if (!s.case_name.empty()) {
        s.mission_name = s.case_name;
    } else {
        s.mission_name = "";
    }

    return s;
}

ActivityDisplay FormatActivity(const PresenceSnapshot& snap, bool show_file, bool show_mission,
                               bool show_diff, bool show_area, bool show_boss, bool show_time) {
    ActivityDisplay act;
    act.large_image = "astral_chain_logo";
    act.large_text = "Astral Chain";
    act.small_image = "neuron_badge";
    act.small_text = snap.difficulty_name.empty() ? "Neuron Officer" : snap.difficulty_name;

    if (!snap.in_game) {
        act.details = "No Menu Principal";
        act.state = "Preparando-se para o dever";
        return act;
    }

    struct AreaKeyEntry {
        unsigned id;
        const char* key;
    };
    static const AreaKeyEntry k_area_keys[] = {
        {0x5000, "01_hypermegafloat_ark"}, {0x5010, "02_astral_plane"},
        {0x5100, "03_restricted_district"}, {0x5110, "04_aegis_research_institute"},
        {0x5120, "05_zone_09"}, {0x5130, "06_sector_v"},
        {0x5140, "07_zone_10"}, {0x5150, "08_ark_police_headquarters"},
        {0x5160, "09_special_ops_bureau_hq"}, {0x5170, "10_zone_30"},
        {0x5180, "11_maison_forest"}, {0x5190, "12_zone_32"},
        {0x51a0, "13_ark_mall"}, {0x51b0, "14_zone_33"},
        {0x51c0, "15_harmony_square"}, {0x51d0, "16_harmony_square_station"},
        {0x51e0, "17_zone_36"}, {0x51f0, "18_central_city"},
        {0x5200, "19_eastside_highway"}, {0x5210, "20_rayleigh_plaza"},
        {0x5220, "21_grand_avenue"}, {0x5230, "22_ark_sewer_management"}
    };
    for (const auto& ak : k_area_keys) {
        if (ak.id == snap.area_id) {
            act.large_image = ak.key;
            if (!snap.area_name.empty()) act.large_text = snap.area_name;
            break;
        }
    }

    // File / Chapter for small_image
    if (snap.file_num >= 1 && snap.file_num <= 11) {
        char fkey[16];
        std::snprintf(fkey, sizeof fkey, "file_%02u", snap.file_num);
        act.small_image = fkey;
        char ftitle[32];
        std::snprintf(ftitle, sizeof ftitle, "File %02u", snap.file_num);
        act.small_text = ftitle;
    }

    // Details Line: File / Chapter + Mission or Difficulty
    std::string d;
    if (show_file && snap.file_num > 0) {
        char fbuf[32];
        std::snprintf(fbuf, sizeof fbuf, "File %02d", snap.file_num);
        d = fbuf;
        if (!snap.file_name.empty()) {
            d += ": " + snap.file_name;
        }
    }

    if (show_mission && !snap.mission_name.empty()) {
        if (!d.empty()) d += " | ";
        d += snap.mission_name;
    } else if (show_diff && !snap.difficulty_name.empty()) {
        if (!d.empty()) d += " | ";
        d += snap.difficulty_name;
    }
    act.details = d.empty() ? "Em Patrulha" : d;

    // State Line: Combat / Boss / Area
    std::string st;
    if (show_boss && snap.boss_active && !snap.boss_name.empty()) {
        if (show_area && !snap.area_name.empty()) st = snap.area_name + " | ";
        st += "Lutando: " + snap.boss_name;
    } else if (show_area && !snap.area_name.empty()) {
        st = snap.area_name + " (Explorando)";
    } else if (snap.battle_state > 0) {
        st = "Em Combate";
    } else {
        st = "Explorando a Arca";
    }
    act.state = st;

    // Time Text
    if (show_time) {
        act.has_time = true;
        uint64_t elapsed = GetSessionElapsedSeconds();
        uint64_t hrs = elapsed / 3600;
        uint64_t min = (elapsed / 60) % 60;
        uint64_t sec = elapsed % 60;
        char tbuf[64];
        if (hrs > 0) {
            std::snprintf(tbuf, sizeof tbuf, "%02llu:%02llu:%02llu decorridos",
                          static_cast<unsigned long long>(hrs),
                          static_cast<unsigned long long>(min),
                          static_cast<unsigned long long>(sec));
        } else {
            std::snprintf(tbuf, sizeof tbuf, "%02llu:%02llu decorridos",
                          static_cast<unsigned long long>(min),
                          static_cast<unsigned long long>(sec));
        }
        act.time_text = tbuf;
    }

    return act;
}

} // namespace AcGameData
