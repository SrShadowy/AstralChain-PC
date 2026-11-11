// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Self-test for Astral Chain Discord Rich Presence formatting logic.
#include <cassert>
#include <cstdio>
#include <string>

#include "additions/rpc/ac_game_data.h"

extern "C" unsigned long long ac_reinfo_main_base(void) { return 0x7000010000ull; }
extern "C" unsigned long long ac_reinfo_guest_lo(void) { return 0x7000000000ull; }
extern "C" unsigned long long ac_reinfo_guest_hi(void) { return 0x7320000000ull; }

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

int main() {
    std::printf("[test] Running Astral Chain Rich Presence format test...\n");

    // Case 1: In title menu / not in game
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = false;
        auto act = AcGameData::FormatActivity(s, true, true, true, true, true, true);
        TEST_ASSERT(act.details == "No Menu Principal", "Title menu details mismatch");
        TEST_ASSERT(act.state == "Preparando-se para o dever", "Title menu state mismatch");
    }

    // Case 2: In-game File 03, Mission Maison Forest Search, exploring Sector V
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = true;
        s.file_num = 3;
        s.file_name = "Link";
        s.mission_name = "Maison Forest Search";
        s.area_name = "Sector V";
        s.difficulty_name = "Standard";
        s.boss_active = false;

        auto act = AcGameData::FormatActivity(s, true, true, true, true, true, false);
        TEST_ASSERT(act.details == "File 03: Link | Maison Forest Search", "File 03 details mismatch");
        TEST_ASSERT(act.state == "Sector V (Explorando)", "File 03 state mismatch");
    }

    // Case 3: Boss fight active
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = true;
        s.file_num = 11;
        s.file_name = "Reckoning";
        s.mission_name = "Final Battle";
        s.area_name = "Astral Plane";
        s.boss_name = "NOAH PRIME";
        s.boss_active = true;
        s.difficulty_name = "Hardcore";

        auto act = AcGameData::FormatActivity(s, true, true, true, true, true, false);
        TEST_ASSERT(act.details == "File 11: Reckoning | Final Battle", "Boss fight details mismatch");
        TEST_ASSERT(act.state == "Astral Plane | Lutando: NOAH PRIME", "Boss fight state mismatch");
    }

    // Case 4: Custom toggles (File disabled, Mission disabled, show difficulty)
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = true;
        s.file_num = 1;
        s.file_name = "Startup";
        s.mission_name = "Hot Pursuit";
        s.area_name = "Eastside Highway";
        s.difficulty_name = "Casual";
        s.boss_active = false;

        auto act = AcGameData::FormatActivity(s, false, false, true, true, false, false);
        TEST_ASSERT(act.details == "Casual", "Toggle details mismatch");
        TEST_ASSERT(act.state == "Eastside Highway (Explorando)", "Toggle state mismatch");
    }

    // Case 5: Exploration mode simulation (File 03 Maison Forest, mission_id == 0)
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = true;
        s.scene_id = 0x1301;
        s.file_num = 3;
        s.file_name = "Link";
        s.case_name = "Missing";
        s.mission_name = "Missing";
        s.area_id = 0x5180;
        s.area_name = "Maison Forest";
        s.difficulty_name = "Casual";
        s.boss_active = false;

        auto act = AcGameData::FormatActivity(s, true, true, true, true, true, false);
        TEST_ASSERT(act.details == "File 03: Link | Missing", "File 03 investigation details mismatch");
        TEST_ASSERT(act.state == "Maison Forest (Explorando)", "File 03 investigation state mismatch");
        TEST_ASSERT(act.state.find("Unknown") == std::string::npos, "Area state must not say Unknown");
    }

    // Case 6: Fallback when area is truly unknown (never display 'Unknown Zone')
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = true;
        s.file_num = 0;
        s.area_name = "";

        auto act = AcGameData::FormatActivity(s, true, true, true, true, false, false);
        TEST_ASSERT(act.state == "Explorando a Arca", "Empty area fallback mismatch");
        TEST_ASSERT(act.state.find("Unknown") == std::string::npos, "Empty area must not say Unknown");
    }

    std::printf("[test] All RPC formatting tests PASSED!\n");
    return 0;
}
