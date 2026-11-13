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

    // Case 5: Real File 03 Astral Plane (user screenshot: Rescue Isabella, stale Maison Forest trigger 0x8404)
    {
        AcGameData::PresenceSnapshot s{};
        s.phase_id = 0x0a01;
        s.scene_id = 0x6000;
        s.raw_area_id = 0x8404; // stale Maison Forest trigger before portal entry
        s.scenario_step = 73;   // file03_1200 -> 0x0309 "Rescue Isabella."
        s.difficulty_name = "Casual";
        AcGameData::ResolveSnapshot(s);

        TEST_ASSERT(s.in_game, "File 03 Astral Plane must be in_game");
        TEST_ASSERT(s.file_num == 3 && s.file_name == "Link", "File 03 resolution mismatch");
        TEST_ASSERT(s.area_id == 0x5010 && s.area_name == "Astral Plane", "Astral Plane area mismatch");
        TEST_ASSERT(s.mission_name == "Rescue Isabella.", "Rescue Isabella objective mismatch");

        auto act = AcGameData::FormatActivity(s, true, true, true, true, true, false);
        TEST_ASSERT(act.details == "File 03: Link | Rescue Isabella.", "Astral Plane details mismatch");
        TEST_ASSERT(act.state == "Astral Plane (Explorando)", "Astral Plane state mismatch");
        TEST_ASSERT(act.large_image == "02_astral_plane", "Astral Plane icon mismatch");
        TEST_ASSERT(act.small_image == "file_03", "File 03 badge mismatch");
    }

    // Case 6: Real File 03 Police HQ (scene_id = 0x1903: stage 0x19 Police HQ, file 0x03)
    {
        AcGameData::PresenceSnapshot s{};
        s.phase_id = 0x0900;
        s.scene_id = 0x1903;
        s.scenario_hash = 0x3cdaaeffu; // file03_100 -> 0x0301 "Go to the Command Room."
        AcGameData::ResolveSnapshot(s);

        TEST_ASSERT(s.file_num == 3 && s.file_name == "Link", "Police HQ File 03 BCD mismatch");
        TEST_ASSERT(s.area_id == 0x5150 && s.area_name == "Ark National Police Headquarters", "Police HQ area mismatch");
        TEST_ASSERT(s.mission_name == "Go to the Command Room.", "Police HQ objective mismatch");
    }

    // Case 7: Real File 03 Maison Forest (scene_id = 0x1a03, raw_area_id = 0x8404)
    {
        AcGameData::PresenceSnapshot s{};
        s.phase_id = 0x0840;
        s.scene_id = 0x1a03;
        s.raw_area_id = 0x8404;
        s.scenario_hash = 0x5caafc87u; // file03_1700 -> 0x0311 "Meet up with Akira."
        AcGameData::ResolveSnapshot(s);

        TEST_ASSERT(s.file_num == 3 && s.file_name == "Link", "Maison Forest File 03 mismatch");
        TEST_ASSERT(s.area_id == 0x5180 && s.area_name == "Maison Forest", "Maison Forest area mismatch");
        TEST_ASSERT(s.mission_name == "Meet up with Akira.", "Maison Forest objective mismatch");
    }

    // Case 8: File 10 Ark Sewer Management (raw_area_id = 0x5000 in phase 0x0500 must map to 0x5230)
    {
        AcGameData::PresenceSnapshot s{};
        s.phase_id = 0x0500;
        s.scene_id = 0x1510;
        s.raw_area_id = 0x5000;
        AcGameData::ResolveSnapshot(s);

        TEST_ASSERT(s.file_num == 10 && s.file_name == "Madness", "File 10 BCD mismatch");
        TEST_ASSERT(s.area_id == 0x5230 && s.area_name == "Ark Sewer Management", "Sewer area 0x5000 collision mismatch");
    }

    // Case 9: Fallback when area is truly unknown (never display 'Unknown Zone')
    {
        AcGameData::PresenceSnapshot s{};
        s.in_game = true;
        s.file_num = 0;
        s.area_name = "";

        auto act = AcGameData::FormatActivity(s, true, true, true, true, false, false);
        TEST_ASSERT(act.state == "Explorando a Arca", "Empty area fallback mismatch");
        TEST_ASSERT(act.state.find("Unknown") == std::string::npos, "Empty area must not say Unknown");
    }

    // Case 10: Active Legion (0..4) maps to small_image legion_* and small_text "<Legion> | File XX"
    {
        const char* expected_keys[5] = {
            "legion_sword",
            "legion_arm",
            "legion_beast",
            "legion_axe",
            "legion_arrow",
        };
        const char* expected_names[5] = {
            "Sword Legion | File 03",
            "Arm Legion | File 03",
            "Beast Legion | File 03",
            "Axe Legion | File 03",
            "Arrow Legion | File 03",
        };
        for (int lid = 0; lid < 5; ++lid) {
            AcGameData::PresenceSnapshot s{};
            s.phase_id = 0x0840;
            s.scene_id = 0x1a03;
            s.legion_id = lid;
            AcGameData::ResolveSnapshot(s);

            auto act = AcGameData::FormatActivity(s, true, true, true, true, false, false);
            TEST_ASSERT(act.small_image == expected_keys[lid], "Legion small_image mismatch");
            TEST_ASSERT(act.small_text == expected_names[lid], "Legion small_text mismatch");
        }
    }

    // Case 11: Legion offsets & unsummoned/fallback behaviour
    {
        static_assert(AcGameData::kSlotPlayerTable == 0xdbb680, "kSlotPlayerTable mismatch");
        static_assert(AcGameData::kOffPlayerLegionActive == 0xf48, "kOffPlayerLegionActive mismatch");
        static_assert(AcGameData::kOffPlayerLegionSelected == 0xf38, "kOffPlayerLegionSelected mismatch");
        static_assert(AcGameData::kOffSaveLegion == 0x74ef8, "kOffSaveLegion mismatch");

        AcGameData::PresenceSnapshot s{};
        s.phase_id = 0x0840;
        s.scene_id = 0x1a03;
        s.legion_id = 5; // Unsummoned / dismissed state from page_70005ec
        AcGameData::ResolveSnapshot(s);

        auto act = AcGameData::FormatActivity(s, true, true, true, true, false, false);
        TEST_ASSERT(act.small_image == "file_03", "Unsummoned legion must fall back to file icon");
        TEST_ASSERT(act.small_text == "File 03", "Unsummoned legion text mismatch");
    }

    std::printf("[test] All RPC formatting tests PASSED!\n");
    return 0;
}
