// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Lossless INI reader/writer and atomic file persistence for userdata/launcher.ini.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ac::config {

struct IniEntry {
    std::string section;
    std::string key;
    std::string value;
};

struct FileStamp {
    bool exists = false;
    int64_t mtime_sec = 0;
    int64_t mtime_nsec = 0;
    uint64_t size = 0;
    uint64_t fnv1a = 0;
};

// Reads raw bytes from path; returns true if the file exists and was read.
bool ReadFileBytes(const std::string& path, std::string& out_bytes);

// Returns OS stat stamp + content FNV-1a hash (computes hash only if needed when caller checks stamp).
bool StatFile(const std::string& path, FileStamp& out_stamp);

// Computes 64-bit FNV-1a over bytes.
uint64_t HashBytesFnv1a(const std::string& bytes);

// Parses entries from INI file bytes (QSettings::IniFormat compatible).
std::vector<IniEntry> ParseIni(const std::string& bytes);

// Looks up key inside section (case-sensitive, returns nullptr if absent).
const std::string* FindIniValue(const std::vector<IniEntry>& entries,
                                const std::string& section,
                                const std::string& key);

// Merges desired key/value updates into raw INI content, preserving unknown sections, keys, and formatting.
std::string MergeIniUpdates(const std::string& existing_bytes,
                            const std::vector<IniEntry>& updates);

// Atomically writes bytes via tmp file + rename, ensuring parent directory exists. Returns updated stamp.
bool WriteFileAtomic(const std::string& path, const std::string& bytes, FileStamp& out_stamp);

} // namespace ac::config
