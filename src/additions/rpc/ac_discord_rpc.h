// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Native Linux Discord IPC Rich Presence client.
#pragma once

#include <atomic>
#include <string>

#include "extra/rpc/ac_game_data.h"

namespace AcDiscordRpc {

struct Settings {
    std::atomic<bool> enabled{true};
    std::atomic<bool> show_file{true};
    std::atomic<bool> show_mission{true};
    std::atomic<bool> show_area{true};
    std::atomic<bool> show_boss{true};
    std::atomic<bool> show_difficulty{true};
    std::atomic<bool> show_time{true};
    std::atomic<bool> window_open{false};
    std::string client_id = "1568955734606086196";
};

Settings& GetSettings();
void Init();
void Shutdown();
bool IsConnected();
const char* GetStatusText();
AcGameData::ActivityDisplay GetCurrentActivity();

} // namespace AcDiscordRpc
