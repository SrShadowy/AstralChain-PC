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
    if (!guest_range(a, n)) {
        return false;
    }
    std::memcpy(out, reinterpret_cast<const void *>(static_cast<uintptr_t>(a)), n);
    return true;
}

bool slot64(unsigned off, unsigned long long *out) {
    unsigned long long ptr = 0;
    const unsigned long long base = ac_reinfo_main_base();
    if (!base || !read_raw(base + off, &ptr, sizeof ptr)) {
        return false;
    }
    if (!guest_range(ptr, sizeof ptr) || (ptr & 3)) {
        return false;
    }
    *out = ptr;
    return true;
}

template <typename T>
bool slot_val(unsigned off, T *out) {
    unsigned long long ptr = 0;
    return slot64(off, &ptr) && read_raw(ptr, out, sizeof *out);
}

template <size_t N>
const char* lookup_name(const AcNameEntry (&t)[N], unsigned id) {
    for (size_t i = 0; i < N; ++i) {
        if (t[i].id == id) {
            return t[i].name;
        }
    }
    return nullptr;
}

template <size_t N>
void resolve_name(const AcNameEntry (&t)[N], unsigned id, std::string& out) {
    if (id != 0 && out.empty()) {
        if (const char* name = lookup_name(t, id)) {
            out = name;
        }
    }
}

template <size_t N>
const AcSceneEntry* lookup_scene(const AcSceneEntry (&t)[N], unsigned scene_id) {
    for (size_t i = 0; i < N; ++i) {
        if (t[i].scene_id == scene_id) {
            return &t[i];
        }
    }
    return nullptr;
}

bool read_scenario_group_step(unsigned long long mgr, unsigned idx_off, int *out_step) {
    int32_t grp_idx = -1;
    if (!read_raw(mgr + idx_off, &grp_idx, sizeof grp_idx)) {
        return false;
    }
    if (grp_idx < 0 || grp_idx >= kMaxScenGroups) {
        return false;
    }
    unsigned long long grp_ptr = 0;
    if (!read_raw(mgr + kOffScenGroups + static_cast<unsigned>(grp_idx) * 8, &grp_ptr, sizeof grp_ptr)) {
        return false;
    }
    int32_t step = -1;
    if (!read_raw(grp_ptr + kOffScenStepIdx, &step, sizeof step)) {
        return false;
    }
    *out_step = static_cast<int>(step);
    return true;
}

int read_active_legion(unsigned long long save_ptr) {
    unsigned long long ptable = 0;
    if (slot64(kSlotPlayerTable, &ptable) && ptable) {
        unsigned long long pl = 0;
        if (read_raw(ptable, &pl, sizeof pl) && pl) {
            int32_t leg = -1;
            if (read_raw(pl + kOffPlayerLegionActive, &leg, sizeof leg) &&
                leg >= kMinLegionId && leg <= kMaxLegionId) {
                return leg;
            }
            if (read_raw(pl + kOffPlayerLegionSelected, &leg, sizeof leg) &&
                leg >= kMinLegionId && leg <= kMaxLegionId) {
                return leg;
            }
        }
    }
    if (save_ptr) {
        int32_t s_leg = -1;
        if (read_raw(save_ptr + kOffSaveLegion, &s_leg, sizeof s_leg) &&
            s_leg >= kMinLegionId && s_leg <= kMaxLegionId) {
            return s_leg;
        }
    }
    return -1;
}


unsigned map_trigger_area(unsigned raw_area, uint32_t phase_id, int file_num) {
    if (raw_area == 0 || raw_area >= 0x10000 || (phase_id >= 0x0a00u && phase_id < 0x0c00u)) {
        return 0;
    }
    if (phase_id != 0) {
        unsigned p_hi = (phase_id >> 8) & 0x0F;
        unsigned a_hi = (raw_area >> 12) & 0x0F;
        bool mismatch = (a_hi != 0 && a_hi != p_hi) ||
                        (phase_id == 0x0840u && (raw_area >> 8) != 0x84u) ||
                        (phase_id == 0x0800u && (raw_area >> 8) == 0x84u);
        if (mismatch) {
            return 0;
        }
    }
    unsigned hi = (raw_area >> 8) & 0xFF;
    if (hi == 0x00) {
        return (raw_area >= 0x0002) ? kAreaRayleighPlaza : kAreaGrandAvenue;
    }
    if (hi >= 0x11 && hi <= 0x15) {
        if (raw_area == 0x1302 || raw_area == 0x1303 || file_num == 7) {
            return kAreaRayleighPlaza;
        }
        return kAreaGrandAvenue;
    }
    if (hi == 0x50 && (phase_id == 0x0500u || raw_area == 0x5001u || file_num == 10)) {
        return kAreaArkSewer;
    }
    for (const auto& r : kTriggerAreaRanges) {
        if (hi >= r.hi_min && hi <= r.hi_max) {
            return r.area_id;
        }
    }
    if (lookup_name(k_ac_areas, raw_area)) {
        return raw_area;
    }
    return 0;
}

unsigned resolve_area_id(const PresenceSnapshot& s, const AcSceneEntry* sc, const AcScenarioStep* step_info) {
    if ((s.scene_id & 0xF000u) == 0x6000u || (s.phase_id >= 0x0a00u && s.phase_id < 0x0c00u)) {
        return kAreaAstralPlane;
    }
    if (unsigned a = map_trigger_area(s.raw_area_id, s.phase_id, s.file_num); a != 0) {
        return a;
    }
    if (unsigned a = map_trigger_area(s.area_id, s.phase_id, s.file_num); a != 0) {
        return a;
    }
    if (unsigned a = MapSceneOrPhaseArea(s.scene_id, s.phase_id, s.file_num); a != 0) {
        return a;
    }
    if (sc && sc->area_id != 0) {
        return sc->area_id;
    }
    if (step_info && step_info->area_id != 0) {
        return step_info->area_id;
    }
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

void ResolveSnapshot(PresenceSnapshot& s) {
    constexpr size_t kStepCount = sizeof(k_ac_scenario_steps) / sizeof(k_ac_scenario_steps[0]);
    const AcScenarioStep* step_info = nullptr;
    if (s.scenario_step > 0 && static_cast<size_t>(s.scenario_step) < kStepCount) {
        step_info = &k_ac_scenario_steps[s.scenario_step];
    } else if (s.scenario_hash != 0) {
        for (size_t i = 1; i < kStepCount; ++i) {
            if (k_ac_scenario_steps[i].step_hash == s.scenario_hash) {
                step_info = &k_ac_scenario_steps[i];
                s.scenario_step = static_cast<int>(i);
                break;
            }
        }
    }

    if (s.file_num < kMinFileNum || s.file_num > kMaxFileNum) {
        if (step_info && step_info->file_num >= kMinFileNum && step_info->file_num <= kMaxFileNum) {
            s.file_num = step_info->file_num;
        } else if (int fm = DeriveFileFromMission(s.mission_id); fm != 0) {
            s.file_num = fm;
        } else {
            s.file_num = DeriveFileFromScene(s.scene_id);
        }
    }
    if (s.file_num >= kMinFileNum && s.file_num <= kMaxFileNum) {
        resolve_name(k_ac_files, static_cast<unsigned>(s.file_num), s.file_name);
    }

    if (step_info && step_info->purpose_id != 0 && s.purpose_id == 0) {
        s.purpose_id = step_info->purpose_id;
    }
    resolve_name(k_ac_purposes, s.purpose_id, s.case_name);

    const AcSceneEntry* sc = nullptr;
    if (s.scene_id != 0) {
        sc = lookup_scene(k_ac_scenes, s.scene_id);
        if (s.case_name.empty() && sc && sc->case_name) {
            s.case_name = sc->case_name;
        }
    }

    unsigned resolved_area = resolve_area_id(s, sc, step_info);
    if (resolved_area != 0) {
        s.area_id = resolved_area;
        resolve_name(k_ac_areas, s.area_id, s.area_name);
    }

    if (s.legion_id >= kMinLegionId && s.legion_id <= kMaxLegionId && s.legion_name.empty()) {
        if (const char* lname = lookup_name(k_ac_legions, static_cast<unsigned>(s.legion_id))) {
            s.legion_name = lname;
        } else {
            s.legion_name = LegionToStr(s.legion_id);
        }
    }

    resolve_name(k_ac_bosses, s.boss_id, s.boss_name);
    resolve_name(k_ac_missions, s.mission_id, s.mission_name);
    if (s.mission_name.empty() && !s.case_name.empty()) {
        s.mission_name = s.case_name;
    }
    s.in_game = (s.total_playtime_s > 0 || s.file_num > 0 || s.scene_id != 0 ||
                 s.area_id != 0 || s.mission_id != 0);
}

PresenceSnapshot GetSnapshot() {
    Init();
    PresenceSnapshot s;
    s.session_start_unix_s = g_session_start_unix;
    if (!ac_reinfo_main_base()) {
        return s;
    }

    (void)slot_val(kSlotArea, &s.raw_area_id);
    if (s.raw_area_id == 0xFFFFFFFFu || s.raw_area_id >= 0x10000u) {
        s.raw_area_id = 0;
    }
    (void)slot_val(kSlotSubArea, &s.sub_area);
    (void)slot_val(kSlotMission, &s.mission_id);
    (void)slot_val(kSlotBossId, &s.boss_id);
    (void)slot_val(kSlotPhase, &s.phase_id);

    unsigned b_act = 0;
    if (slot_val(kSlotBossActive, &b_act)) {
        s.boss_active = (b_act == 1);
    }
    (void)slot_val(kSlotBossStage, &s.boss_stage);
    (void)slot_val(kSlotBattleState, &s.battle_state);
    (void)slot_val(kSlotBattleTimer, &s.battle_timer);

    unsigned long long scene_ptr = 0;
    if (slot64(kSlotScene, &scene_ptr)) {
        (void)read_raw(scene_ptr + kOffSceneId, &s.scene_id, sizeof s.scene_id);
    }

    unsigned long long scen_mgr = 0;
    if (slot64(kSlotScenario, &scen_mgr)) {
        int f_step = 0;
        if (read_scenario_group_step(scen_mgr, kOffScenFileGrpIdx, &f_step) &&
            f_step >= kMinFileNum && f_step <= kMaxFileNum) {
            s.file_num = f_step;
        }
        (void)read_scenario_group_step(scen_mgr, kOffScenStepGrpIdx, &s.scenario_step);
    }

    unsigned long long save_ptr = 0;
    if (slot64(kSlotSave, &save_ptr)) {
        (void)read_raw(save_ptr, &s.total_playtime_s, sizeof s.total_playtime_s);
        unsigned diff_raw = 0;
        if (read_raw(save_ptr + kOffSaveDiff, &diff_raw, sizeof diff_raw)) {
            s.difficulty_raw = static_cast<int>(diff_raw);
            s.difficulty_name = DifficultyToStr(s.difficulty_raw);
        }
        if (s.file_num == 0) {
            uint16_t saved_file = 0;
            if (read_raw(save_ptr + kOffSaveFile, &saved_file, sizeof saved_file) &&
                saved_file >= kMinFileNum && saved_file <= kMaxFileNum) {
                s.file_num = static_cast<int>(saved_file);
            }
        }
        uint32_t grp_hash = 0;
        if (read_raw(save_ptr + kOffSaveScenGrp, &grp_hash, sizeof grp_hash) && grp_hash == kScenGrpHash) {
            (void)read_raw(save_ptr + kOffSaveScenStep, &s.scenario_hash, sizeof s.scenario_hash);
        }
    }
    s.legion_id = read_active_legion(save_ptr);

    ResolveSnapshot(s);
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

    for (const auto& ak : kAreaKeys) {
        if (ak.id == snap.area_id) {
            act.large_image = ak.key;
            if (!snap.area_name.empty()) {
                act.large_text = snap.area_name;
            }
            break;
        }
    }

    if (const char* lkey = LegionToIconKey(snap.legion_id)) {
        act.small_image = lkey;
        std::string ltxt = !snap.legion_name.empty() ? snap.legion_name : LegionToStr(snap.legion_id);
        if (snap.file_num >= kMinFileNum && snap.file_num <= kMaxFileNum) {
            char ftitle[32];
            std::snprintf(ftitle, sizeof ftitle, " | File %02d", snap.file_num);
            ltxt += ftitle;
        }
        act.small_text = ltxt;
    } else if (snap.file_num >= kMinFileNum && snap.file_num <= kMaxFileNum) {
        int icon_num = (snap.file_num <= kMaxFileIcon) ? snap.file_num : kMaxFileIcon;
        char fkey[16];
        std::snprintf(fkey, sizeof fkey, "file_%02d", icon_num);
        act.small_image = fkey;
        char ftitle[32];
        std::snprintf(ftitle, sizeof ftitle, "File %02d", snap.file_num);
        act.small_text = ftitle;
    }

    std::string d;
    if (show_file && snap.file_num > 0) {
        char fbuf[32];
        std::snprintf(fbuf, sizeof fbuf, "File %02d", snap.file_num);
        d = fbuf;
        if (!snap.file_name.empty()) {
            d += ": " + snap.file_name;
        }
    }

    std::string suffix;
    if (show_mission && !snap.mission_name.empty()) {
        suffix = snap.mission_name;
    } else if (show_diff && !snap.difficulty_name.empty()) {
        suffix = snap.difficulty_name;
    }
    if (!suffix.empty()) {
        d = d.empty() ? suffix : (d + " | " + suffix);
    }
    act.details = d.empty() ? "Em Patrulha" : d;

    std::string st;
    if (show_boss && snap.boss_active && !snap.boss_name.empty()) {
        if (show_area && !snap.area_name.empty()) {
            st = snap.area_name + " | ";
        }
        st += "Lutando: " + snap.boss_name;
    } else if (show_area && !snap.area_name.empty()) {
        st = snap.area_name + " (Explorando)";
    } else if (snap.battle_state > 0) {
        st = "Em Combate";
    } else {
        st = "Explorando a Arca";
    }
    act.state = st;

    if (show_time) {
        act.has_time = true;
        unsigned long long elapsed = GetSessionElapsedSeconds();
        unsigned long long hrs = elapsed / 3600;
        unsigned long long min = (elapsed / 60) % 60;
        unsigned long long sec = elapsed % 60;
        char tbuf[64];
        if (hrs > 0) {
            std::snprintf(tbuf, sizeof tbuf, "%02llu:%02llu:%02llu decorridos", hrs, min, sec);
        } else {
            std::snprintf(tbuf, sizeof tbuf, "%02llu:%02llu decorridos", min, sec);
        }
        act.time_text = tbuf;
    }

    return act;
}

} // namespace AcGameData
