// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Bidirectional runtime <-> userdata/launcher.ini synchronization service.
#include "additions/config/ac_config_sync.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "additions/config/ac_config_sections.h"
#include "additions/config/ac_config_serialize.h"
#include "additions/config/ac_ini_store.h"
#include "additions/toggles/ac_toggles.h"

namespace {

constexpr const char* kDefaultLauncherIniPath = "userdata/launcher.ini";
constexpr const char* kLegacyLarecompIniPath = "larecomp.ini";
constexpr auto kPollInterval = std::chrono::milliseconds(150);

std::mutex g_sync_mutex;
std::string g_ini_path = kDefaultLauncherIniPath;
bool g_initialized = false;
ac::config::FileStamp g_last_stamp{};
ac::config::SyncState g_sync_state{};
std::chrono::steady_clock::time_point g_last_poll_tp{};

bool StampMetadataEqual(const ac::config::FileStamp& a, const ac::config::FileStamp& b) {
    return a.exists == b.exists &&
           a.mtime_sec == b.mtime_sec &&
           a.mtime_nsec == b.mtime_nsec &&
           a.size == b.size;
}

void ApplyEntriesLocked(const std::vector<ac::config::IniEntry>& entries,
                        bool boot,
                        bool legacy_fallback) {
    ac::config::ApplyGraphicsSection(entries, boot, legacy_fallback, g_sync_state);
    ac::config::ApplyAudioSection(entries, boot);
    ac::config::ApplyMouseSection(entries, boot);
    ac::config::ApplyGameSection(entries, boot, legacy_fallback, g_sync_state);
    ac::config::ApplyOverlayAndRpcSections(entries, boot);
    ac::config::ApplyControlsSection(entries);
}

void SaveLocked(void) {
    std::string existing_bytes;
    ac::config::ReadFileBytes(g_ini_path, existing_bytes);
    const auto updates = ac::config::BuildIniUpdates(g_sync_state);
    const std::string merged = ac::config::MergeIniUpdates(existing_bytes, updates);
    if (merged == existing_bytes) {
        return;
    }
    ac::config::FileStamp written_stamp{};
    if (ac::config::WriteFileAtomic(g_ini_path, merged, written_stamp)) {
        g_last_stamp = written_stamp;
        g_sync_state.win_w = g_extra_win_w > 0 ? g_extra_win_w : g_sync_state.win_w;
        g_sync_state.win_h = g_extra_win_h > 0 ? g_extra_win_h : g_sync_state.win_h;
    }
}

void PollLocked(bool force) {
    if (!g_initialized) {
        return;
    }
    if (g_extra_config_dirty) {
        g_extra_config_dirty = 0;
        SaveLocked();
    }
    const auto now = std::chrono::steady_clock::now();
    if (!force && (now - g_last_poll_tp) < kPollInterval) {
        return;
    }
    g_last_poll_tp = now;

    ac::config::FileStamp cur_stamp{};
    ac::config::StatFile(g_ini_path, cur_stamp);
    if (!cur_stamp.exists) {
        return;
    }
    if (StampMetadataEqual(cur_stamp, g_last_stamp)) {
        return;
    }

    std::string bytes;
    if (!ac::config::ReadFileBytes(g_ini_path, bytes)) {
        return;
    }
    cur_stamp.fnv1a = ac::config::HashBytesFnv1a(bytes);
    if (g_last_stamp.exists && cur_stamp.fnv1a == g_last_stamp.fnv1a) {
        g_last_stamp = cur_stamp;
        return;
    }
    g_last_stamp = cur_stamp;
    const auto entries = ac::config::ParseIni(bytes);
    ApplyEntriesLocked(entries, false, false);
}

} // namespace

extern "C" {

void ac_config_sync_set_path(const char* path) {
    if (!path || !*path) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_sync_mutex);
    g_ini_path = path;
}

void ac_config_sync_init(void) {
    std::lock_guard<std::mutex> lock(g_sync_mutex);
    if (g_initialized) {
        return;
    }
    g_initialized = true;
    g_last_poll_tp = std::chrono::steady_clock::now();

    std::string bytes;
    bool legacy_fallback = false;
    if (ac::config::ReadFileBytes(g_ini_path, bytes)) {
        ac::config::StatFile(g_ini_path, g_last_stamp);
        g_last_stamp.fnv1a = ac::config::HashBytesFnv1a(bytes);
    } else if (ac::config::ReadFileBytes(kLegacyLarecompIniPath, bytes)) {
        legacy_fallback = true;
    }
    const auto entries = ac::config::ParseIni(bytes);
    ApplyEntriesLocked(entries, true, legacy_fallback);
}

void ac_config_sync_poll(void) {
    std::lock_guard<std::mutex> lock(g_sync_mutex);
    PollLocked(false);
}

void ac_config_sync_poll_force(void) {
    std::lock_guard<std::mutex> lock(g_sync_mutex);
    PollLocked(true);
}

void ac_config_sync_save(void) {
    std::lock_guard<std::mutex> lock(g_sync_mutex);
    if (!g_initialized) {
        return;
    }
    g_extra_config_dirty = 0;
    SaveLocked();
}

void ac_config_sync_init_hook(void) {
    ac_config_sync_init();
}

void ac_config_sync_save_hook(void) {
    ac_config_sync_save();
}

} // extern "C"
