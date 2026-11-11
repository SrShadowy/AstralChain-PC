// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Astral Chain game state reader for Discord RPC and in-game UI.
#pragma once

#include <cstdint>
#include <string>

namespace AcGameData {

struct PresenceSnapshot {
    bool in_game = false;
    unsigned area_id = 0;
    unsigned sub_area = 0;
    std::string area_name;
    uint32_t scene_id = 0;       // Live scene id from slot 0xdb84c8 (+0x180)
    std::string case_name;       // e.g. "Missing", "Sortie", "Rush"
    unsigned mission_id = 0;
    int file_num = 0;            // 1..12 (0 = unknown)
    std::string file_name;       // e.g. "Startup", "Link", "Reckoning"
    std::string mission_name;    // e.g. "Maison Forest Search"
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
PresenceSnapshot GetSnapshot();
ActivityDisplay FormatActivity(const PresenceSnapshot& snap, bool show_file, bool show_mission,
                               bool show_diff, bool show_area, bool show_boss, bool show_time);
uint64_t GetSessionElapsedSeconds();

} // namespace AcGameData
