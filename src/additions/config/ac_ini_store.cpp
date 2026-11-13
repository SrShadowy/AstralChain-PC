// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Lossless INI reader/writer and atomic file persistence for userdata/launcher.ini.
#include "additions/config/ac_ini_store.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace ac::config {
namespace {

constexpr uint64_t kFnv1aOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnv1aPrime = 1099511628211ULL;
constexpr mode_t kDefaultDirMode = 0755;

std::string Trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string Unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            std::string line = text.substr(pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
            break;
        }
        std::string line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
        pos = nl + 1;
    }
    return lines;
}

bool EnsureParentDirs(const std::string& file_path) {
    size_t pos = 0;
    while ((pos = file_path.find('/', pos + 1)) != std::string::npos) {
        const std::string dir = file_path.substr(0, pos);
        if (dir.empty()) {
            continue;
        }
        if (mkdir(dir.c_str(), kDefaultDirMode) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

bool ParseLineSectionOrKey(const std::string& trimmed,
                           std::string& out_section,
                           std::string& out_key,
                           std::string& out_val) {
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
        return false;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
        out_section = Trim(trimmed.substr(1, trimmed.size() - 2));
        out_key.clear();
        return true;
    }
    const size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
        return false;
    }
    out_key = Trim(trimmed.substr(0, eq));
    out_val = Unquote(Trim(trimmed.substr(eq + 1)));
    out_section.clear();
    return !out_key.empty();
}

} // namespace

bool ReadFileBytes(const std::string& path, std::string& out_bytes) {
    out_bytes.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    struct stat st{};
    if (fstat(fileno(f), &st) == 0 && st.st_size > 0) {
        out_bytes.reserve(static_cast<size_t>(st.st_size));
    }
    char buf[4096];
    while (true) {
        const size_t n = std::fread(buf, 1, sizeof(buf), f);
        if (n > 0) {
            out_bytes.append(buf, n);
        }
        if (n < sizeof(buf)) {
            break;
        }
    }
    std::fclose(f);
    return true;
}

bool StatFile(const std::string& path, FileStamp& out_stamp) {
    out_stamp = FileStamp{};
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    out_stamp.exists = true;
    out_stamp.mtime_sec = static_cast<int64_t>(st.st_mtim.tv_sec);
    out_stamp.mtime_nsec = static_cast<int64_t>(st.st_mtim.tv_nsec);
    out_stamp.size = static_cast<uint64_t>(st.st_size);
    return true;
}

uint64_t HashBytesFnv1a(const std::string& bytes) {
    uint64_t h = kFnv1aOffsetBasis;
    for (unsigned char c : bytes) {
        h ^= static_cast<uint64_t>(c);
        h *= kFnv1aPrime;
    }
    return h;
}

std::vector<IniEntry> ParseIni(const std::string& bytes) {
    std::vector<IniEntry> entries;
    std::string cur_section;
    for (const std::string& raw : SplitLines(bytes)) {
        std::string sec;
        std::string key;
        std::string val;
        if (!ParseLineSectionOrKey(Trim(raw), sec, key, val)) {
            continue;
        }
        if (key.empty()) {
            cur_section = sec;
            continue;
        }
        bool updated = false;
        for (IniEntry& e : entries) {
            if (e.section == cur_section && e.key == key) {
                e.value = val;
                updated = true;
                break;
            }
        }
        if (!updated) {
            entries.push_back(IniEntry{cur_section, key, val});
        }
    }
    return entries;
}

const std::string* FindIniValue(const std::vector<IniEntry>& entries,
                                const std::string& section,
                                const std::string& key) {
    for (const IniEntry& e : entries) {
        if (e.section == section && e.key == key) {
            return &e.value;
        }
    }
    return nullptr;
}

std::string MergeIniUpdates(const std::string& existing_bytes,
                            const std::vector<IniEntry>& updates) {
    std::vector<std::string> lines = SplitLines(existing_bytes);
    std::vector<bool> applied(updates.size(), false);
    std::vector<std::string> out_lines;
    out_lines.reserve(lines.size() + updates.size() + 8);

    std::string cur_section;
    auto flush_missing_in_section = [&](const std::string& sec) {
        if (sec.empty()) {
            return;
        }
        size_t insert_at = out_lines.size();
        while (insert_at > 0 && Trim(out_lines[insert_at - 1]).empty()) {
            --insert_at;
        }
        std::vector<std::string> added;
        for (size_t i = 0; i < updates.size(); ++i) {
            if (!applied[i] && updates[i].section == sec) {
                added.push_back(updates[i].key + "=" + updates[i].value);
                applied[i] = true;
            }
        }
        if (!added.empty()) {
            out_lines.insert(out_lines.begin() + static_cast<ptrdiff_t>(insert_at),
                             added.begin(), added.end());
        }
    };

    for (const std::string& raw : lines) {
        std::string sec;
        std::string key;
        std::string val;
        const std::string trimmed = Trim(raw);
        if (ParseLineSectionOrKey(trimmed, sec, key, val)) {
            if (key.empty()) {
                flush_missing_in_section(cur_section);
                cur_section = sec;
                out_lines.push_back(raw);
                continue;
            }
            bool replaced = false;
            for (size_t i = 0; i < updates.size(); ++i) {
                if (updates[i].section == cur_section && updates[i].key == key) {
                    out_lines.push_back(key + "=" + updates[i].value);
                    applied[i] = true;
                    replaced = true;
                    break;
                }
            }
            if (replaced) {
                continue;
            }
        }
        out_lines.push_back(raw);
    }
    flush_missing_in_section(cur_section);

    std::vector<std::string> pending_sections;
    for (size_t i = 0; i < updates.size(); ++i) {
        if (applied[i]) {
            continue;
        }
        bool seen = false;
        for (const std::string& ps : pending_sections) {
            if (ps == updates[i].section) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            pending_sections.push_back(updates[i].section);
        }
    }

    for (const std::string& sec : pending_sections) {
        if (!out_lines.empty() && !Trim(out_lines.back()).empty()) {
            out_lines.emplace_back("");
        }
        if (!sec.empty()) {
            out_lines.push_back("[" + sec + "]");
        }
        for (size_t i = 0; i < updates.size(); ++i) {
            if (!applied[i] && updates[i].section == sec) {
                out_lines.push_back(updates[i].key + "=" + updates[i].value);
                applied[i] = true;
            }
        }
    }

    std::string result;
    for (const std::string& l : out_lines) {
        result += l;
        result += '\n';
    }
    return result;
}

bool WriteFileAtomic(const std::string& path, const std::string& bytes, FileStamp& out_stamp) {
    if (!EnsureParentDirs(path)) {
        return false;
    }
    const std::string tmp_path = path + ".tmp." + std::to_string(static_cast<long>(getpid()));
    FILE* f = std::fopen(tmp_path.c_str(), "wb");
    if (!f) {
        return false;
    }
    const size_t written = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fflush(f);
    const int close_rc = std::fclose(f);
    if (written != bytes.size() || close_rc != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }
    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }
    StatFile(path, out_stamp);
    out_stamp.fnv1a = HashBytesFnv1a(bytes);
    return true;
}

} // namespace ac::config
