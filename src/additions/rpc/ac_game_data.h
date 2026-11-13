// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Astral Chain game state reader for Discord RPC and in-game UI.
#pragma once

#include <cstdint>
#include <string>

namespace AcGameData {

enum SlotOffset : unsigned {
    kSlotSave          = 0xdb83d8,
    kSlotScene         = 0xdb84c8,
    kSlotPhase         = 0xdb84f8,
    kSlotScenario      = 0xdb8930,
    kSlotPlayerTable         = 0xdbb680,
    kSlotMission             = 0xdbc420,
    kSlotBossActive          = 0xdbc928,
    kSlotBossStage           = 0xdbc930,
    kSlotBattleState         = 0xdbc940,
    kSlotBattleTimer         = 0xdbc948,
    kSlotBossId              = 0xdbc950,
    kSlotArea                = 0xdbcf10,
    kSlotSubArea             = 0xdbcf20,
    kSlotLegionLegacy        = 0xdbf170,
    kOffSceneId              = 0x180,
    kOffSaveFile             = 0x16,
    kOffSaveLegion           = 0x74ef8,
    kOffSaveDiff             = 0xaefa0,
    kOffSaveScenGrp          = 0x14d06c,
    kOffSaveScenStep         = 0x14d070,
    kOffPlayerLegionActive   = 0xf48,
    kOffPlayerLegionSelected = 0xf38,
    kScenGrpHash             = 0x0f154745u,
    kOffScenGroups           = 0x28,
    kOffScenStepIdx          = 0x2c,
    kOffScenFileGrpIdx       = 0x12f0,
    kOffScenStepGrpIdx       = 0x12f4,
};

enum CanonicalArea : unsigned {
    kAreaHypermegafloat = 0x5000,
    kAreaAstralPlane    = 0x5010,
    kAreaRestrictedDist = 0x5100,
    kAreaAri            = 0x5110,
    kAreaZone09         = 0x5120,
    kAreaSectorV        = 0x5130,
    kAreaZone10         = 0x5140,
    kAreaPoliceHq       = 0x5150,
    kAreaSpecialOpsHq   = 0x5160,
    kAreaZone30         = 0x5170,
    kAreaMaisonForest   = 0x5180,
    kAreaWestHarmony    = 0x5190,
    kAreaArkMall        = 0x51a0,
    kAreaNorthHarmony   = 0x51b0,
    kAreaHarmonySquare  = 0x51c0,
    kAreaHarmonyStation = 0x51d0,
    kAreaBypass4        = 0x51e0,
    kAreaNorthJunction  = 0x51f0,
    kAreaEastsideHwy    = 0x5200,
    kAreaRayleighPlaza  = 0x5210,
    kAreaGrandAvenue    = 0x5220,
    kAreaArkSewer       = 0x5230,
};

constexpr int kMaxScenGroups = 16;
constexpr int kMinFileNum    = 1;
constexpr int kMaxFileNum    = 12;
constexpr int kMaxFileIcon   = 11;
constexpr int kMinLegionId   = 0;
constexpr int kMaxLegionId   = 4;

struct LegionKeyEntry {
    int id;
    const char* key;
    const char* name;
};

constexpr LegionKeyEntry kLegionKeys[] = {
    {0, "legion_sword", "Sword Legion"},
    {1, "legion_arm",   "Arm Legion"},
    {2, "legion_beast", "Beast Legion"},
    {3, "legion_axe",   "Axe Legion"},
    {4, "legion_arrow", "Arrow Legion"},
};

constexpr const char* LegionToIconKey(int id) {
    for (const auto& entry : kLegionKeys) {
        if (entry.id == id) {
            return entry.key;
        }
    }
    return nullptr;
}

constexpr const char* LegionToStr(int id) {
    for (const auto& entry : kLegionKeys) {
        if (entry.id == id) {
            return entry.name;
        }
    }
    return "";
}

struct AreaKeyEntry {
    unsigned id;
    const char* key;
};

constexpr AreaKeyEntry kAreaKeys[] = {
    {kAreaHypermegafloat, "01_hypermegafloat_ark"},
    {kAreaAstralPlane,    "02_astral_plane"},
    {kAreaRestrictedDist, "03_restricted_district"},
    {kAreaAri,            "04_aegis_research_institute"},
    {kAreaZone09,         "05_zone_09"},
    {kAreaSectorV,        "06_sector_v"},
    {kAreaZone10,         "07_zone_10"},
    {kAreaPoliceHq,       "08_ark_police_headquarters"},
    {kAreaSpecialOpsHq,   "09_special_ops_bureau_hq"},
    {kAreaZone30,         "10_zone_30"},
    {kAreaMaisonForest,   "11_maison_forest"},
    {kAreaWestHarmony,    "12_zone_32"},
    {kAreaArkMall,        "13_ark_mall"},
    {kAreaNorthHarmony,   "14_zone_33"},
    {kAreaHarmonySquare,  "15_harmony_square"},
    {kAreaHarmonyStation, "16_harmony_square_station"},
    {kAreaBypass4,        "17_zone_36"},
    {kAreaNorthJunction,  "18_central_city"},
    {kAreaEastsideHwy,    "19_eastside_highway"},
    {kAreaRayleighPlaza,  "20_rayleigh_plaza"},
    {kAreaGrandAvenue,    "21_grand_avenue"},
    {kAreaArkSewer,       "22_ark_sewer_management"},
};

struct ScenePhaseAreaEntry {
    unsigned scene_hi;
    uint32_t phase_id;
    unsigned area_id;
};

constexpr ScenePhaseAreaEntry kScenePhaseAreas[] = {
    {0x12, 0x0200, kAreaHarmonySquare},
    {0x13, 0x0300, kAreaSectorV},
    {0x14, 0x0400, kAreaArkMall},
    {0x15, 0x0500, kAreaArkSewer},
    {0x16, 0x0600, kAreaAri},
    {0x18, 0x0800, kAreaZone09},
    {0x19, 0x0900, kAreaPoliceHq},
    {0x1a, 0x0840, kAreaMaisonForest},
    {0x1b, 0x0240, kAreaHarmonyStation},
    {0x1c, 0x0910, kAreaEastsideHwy},
};

struct TriggerRangeAreaEntry {
    unsigned hi_min;
    unsigned hi_max;
    unsigned area_id;
};

constexpr TriggerRangeAreaEntry kTriggerAreaRanges[] = {
    {0x01, 0x01, kAreaPoliceHq},
    {0x02, 0x02, kAreaBypass4},
    {0x03, 0x03, kAreaNorthHarmony},
    {0x04, 0x04, kAreaWestHarmony},
    {0x05, 0x05, kAreaArkMall},
    {0x10, 0x10, kAreaEastsideHwy},
    {0x16, 0x17, kAreaRayleighPlaza},
    {0x20, 0x20, kAreaHarmonySquare},
    {0x21, 0x21, kAreaHarmonyStation},
    {0x22, 0x22, kAreaSectorV},
    {0x30, 0x31, kAreaSectorV},
    {0x32, 0x36, kAreaZone09},
    {0x40, 0x44, kAreaArkMall},
    {0x60, 0x63, kAreaAri},
    {0x80, 0x80, kAreaZone09},
    {0x84, 0x84, kAreaMaisonForest},
    {0x88, 0x89, kAreaZone09},
    {0x90, 0x93, kAreaPoliceHq},
    {0x99, 0x99, kAreaNorthJunction},
};

struct DifficultyEntry {
    int raw;
    const char* name;
};

constexpr DifficultyEntry kDifficulties[] = {
    {-2, "Super Casual"},
    { 0, "Casual"},
    { 1, "Standard"},
    { 2, "Hardcore"},
};

constexpr const char* DifficultyToStr(int raw) {
    for (const auto& d : kDifficulties) {
        if (d.raw == raw) {
            return d.name;
        }
    }
    return "Standard";
}

constexpr int DeriveFileFromMission(unsigned mission_id) {
    if (!mission_id) {
        return 0;
    }
    unsigned hi = (mission_id >> 8) & 0xFF;
    if (hi >= 0x01 && hi <= 0x0c) {
        return static_cast<int>(hi);
    }
    if (hi == 0x21) {
        return 1;
    }
    return 0;
}

constexpr int DeriveFileFromScene(uint32_t scene_id) {
    unsigned hi = (scene_id >> 8) & 0xFF;
    if (hi < 0x11 || hi > 0x1c) {
        return 0;
    }
    unsigned lo = scene_id & 0xFF;
    unsigned tens = (lo >> 4) & 0x0F;
    unsigned ones = lo & 0x0F;
    if (tens > 1 || ones > 9) {
        return 0;
    }
    int f = static_cast<int>(tens * 10 + ones);
    if (f >= kMinFileNum && f <= kMaxFileNum) {
        return f;
    }
    return 0;
}

constexpr unsigned MapSceneOrPhaseArea(uint32_t scene_id, uint32_t phase_id, int file_num) {
    if ((scene_id & 0xF000u) == 0x6000u || (phase_id >= 0x0a00u && phase_id < 0x0c00u)) {
        return kAreaAstralPlane;
    }
    unsigned s_hi = (scene_id >> 8) & 0xFF;
    if (s_hi == 0x11 || phase_id == 0x0100) {
        if (file_num == 7) {
            return kAreaRayleighPlaza;
        }
        if (file_num == 8) {
            return kAreaZone09;
        }
        return kAreaGrandAvenue;
    }
    for (const auto& entry : kScenePhaseAreas) {
        if (s_hi == entry.scene_hi || phase_id == entry.phase_id) {
            return entry.area_id;
        }
    }
    return 0;
}

struct PresenceSnapshot {
    bool in_game = false;
    unsigned area_id = 0;
    unsigned raw_area_id = 0;
    unsigned sub_area = 0;
    std::string area_name;
    uint32_t phase_id = 0;       // Live phase id from slot 0xdb84f8 (e.g. 0x0100..0x0b01)
    uint32_t scene_id = 0;       // Live scene id from slot 0xdb84c8 (+0x180)
    int scenario_step = -1;      // Live File_Scenario step index from slot 0xdb8930
    uint32_t scenario_hash = 0;  // Saved File_Scenario step CRC32 hash fallback
    unsigned purpose_id = 0;     // Resolved CORE_PURPOSE_FILE_%04x id
    std::string case_name;       // e.g. "Rescue Isabella.", "Meet up with Akira."
    unsigned mission_id = 0;
    int file_num = 0;            // 1..12 (0 = title screen / unknown)
    std::string file_name;       // e.g. "Startup", "Link", "Reckoning"
    std::string mission_name;    // Active mission or current story case objective
    int legion_id = -1;          // 0..4 (Sword, Arm, Beast, Axe, Arrow; -1 = unknown)
    std::string legion_name;     // e.g. "Sword Legion"
    unsigned boss_id = 0;
    std::string boss_name;       // e.g. "HOMUNCULUS alpha"
    bool boss_active = false;
    unsigned boss_stage = 0;
    unsigned battle_state = 0;
    float battle_timer = 0.0f;
    int difficulty_raw = 0;      // -2..2
    std::string difficulty_name; // e.g. "Standard", "Casual", "Hardcore"
    uint64_t total_playtime_s = 0;
    int64_t session_start_unix_s = 0;
};

struct ActivityDisplay {
    std::string details;
    std::string state;
    std::string large_image;
    std::string large_text;
    std::string small_image;
    std::string small_text;
    std::string time_text;
    bool has_time = false;
};

void Init();
void ResolveSnapshot(PresenceSnapshot& s);
PresenceSnapshot GetSnapshot();
ActivityDisplay FormatActivity(const PresenceSnapshot& snap, bool show_file, bool show_mission,
                               bool show_diff, bool show_area, bool show_boss, bool show_time);
uint64_t GetSessionElapsedSeconds();

} // namespace AcGameData
