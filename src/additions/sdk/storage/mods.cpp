#include "mods.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ac::mods {

static std::string to_lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static ModCategory detect_category(const std::string& id_lower, bool has_text_folder) {
    if (has_text_folder || id_lower.find("traduc") != std::string::npos ||
        id_lower.find("translat") != std::string::npos || id_lower.find("ptbr") != std::string::npos ||
        id_lower.find("pt_br") != std::string::npos || id_lower.find("espanol") != std::string::npos ||
        id_lower.find("french") != std::string::npos || id_lower.find("german") != std::string::npos ||
        id_lower.find("italian") != std::string::npos || id_lower.find("russian") != std::string::npos ||
        id_lower.find("lang") != std::string::npos) {
        return ModCategory::Translation;
    }
    if (id_lower.find("texture") != std::string::npos || id_lower.find("graphic") != std::string::npos ||
        id_lower.find("shadow") != std::string::npos || id_lower.find("lod") != std::string::npos) {
        return ModCategory::Graphics;
    }
    if (id_lower.find("audio") != std::string::npos || id_lower.find("sound") != std::string::npos ||
        id_lower.find("voice") != std::string::npos || id_lower.find("dub") != std::string::npos ||
        id_lower.find("bgm") != std::string::npos) {
        return ModCategory::Audio;
    }
    if (id_lower.find("ui") != std::string::npos || id_lower.find("glyph") != std::string::npos ||
        id_lower.find("hud") != std::string::npos) {
        return ModCategory::UI;
    }
    return ModCategory::Gameplay;
}

static void parse_mod_ini(const fs::path& ini_path, ModInfo& info) {
    std::ifstream file(ini_path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        val.erase(0, val.find_first_not_of(" \t\r\n"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);

        std::string k_low = to_lower_str(key);
        if (k_low == "name" && !val.empty()) info.name = val;
        else if (k_low == "author" && !val.empty()) info.author = val;
        else if (k_low == "version" && !val.empty()) info.version = val;
        else if (k_low == "description" && !val.empty()) info.description = val;
        else if ((k_low == "language" || k_low == "target_language" || k_low == "lang") && !val.empty()) {
            info.target_language = val;
            info.category = ModCategory::Translation;
        }
        else if (k_low == "category" && !val.empty()) {
            std::string c_low = to_lower_str(val);
            if (c_low == "translation") info.category = ModCategory::Translation;
            else if (c_low == "graphics") info.category = ModCategory::Graphics;
            else if (c_low == "audio") info.category = ModCategory::Audio;
            else if (c_low == "ui") info.category = ModCategory::UI;
            else if (c_low == "gameplay") info.category = ModCategory::Gameplay;
        }
    }
}

static uint64_t compute_dir_size(const fs::path& dir) {
    uint64_t total = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec)) {
            total += entry.file_size(ec);
        }
    }
    return total;
}

std::vector<ModInfo> list_mods(const std::string& mods_root) {
    std::vector<ModInfo> list;
    std::error_code ec;
    fs::path root(mods_root);
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return list;
    }

    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (ec) break;
        if (!entry.is_directory(ec)) continue;

        std::string dirname = entry.path().filename().string();
        if (dirname.empty() || dirname[0] == '.') continue;

        bool has_disabled_ext = (dirname.size() > 9 && dirname.rfind(".disabled") == dirname.size() - 9);
        std::string base_id = has_disabled_ext ? dirname.substr(0, dirname.size() - 9) : dirname;

        fs::path mod_path = entry.path();
        bool has_disabled_file = fs::exists(mod_path / DISABLED_MARKER, ec);
        bool is_enabled = (!has_disabled_ext && !has_disabled_file);

        bool has_romfs = (fs::exists(mod_path / "romfs", ec) || fs::exists(mod_path / "Text", ec) ||
                          fs::exists(mod_path / "ui", ec) || fs::exists(mod_path / "font", ec) ||
                          fs::exists(mod_path / "stage", ec) || fs::exists(mod_path / "actor", ec));
        bool has_exefs = (fs::exists(mod_path / "exefs", ec));

        ModInfo info;
        info.id = base_id;
        info.name = base_id;
        info.path = mod_path.string();
        info.version = "1.0.0";
        info.enabled = is_enabled;
        info.has_romfs = has_romfs;
        info.has_exefs = has_exefs;
        info.category = detect_category(to_lower_str(base_id), fs::exists(mod_path / "Text", ec));
        info.total_size_bytes = compute_dir_size(mod_path);

        fs::path ini_file = mod_path / MOD_CONFIG_INI;
        if (fs::exists(ini_file, ec)) {
            parse_mod_ini(ini_file, info);
        }

        list.push_back(std::move(info));
    }

    std::sort(list.begin(), list.end(), [](const ModInfo& a, const ModInfo& b) {
        return a.id < b.id;
    });
    return list;
}

std::optional<ModInfo> get_mod_info(const std::string& mod_id, const std::string& mods_root) {
    auto mods = list_mods(mods_root);
    for (const auto& m : mods) {
        if (m.id == mod_id) return m;
    }
    return std::nullopt;
}

bool is_mod_enabled(const std::string& mod_id, const std::string& mods_root) {
    auto info = get_mod_info(mod_id, mods_root);
    return info.has_value() && info->enabled;
}

bool set_mod_enabled(const std::string& mod_id, bool enable, const std::string& mods_root) {
    std::error_code ec;
    fs::path root(mods_root);

    fs::path normal_path = root / mod_id;
    fs::path disabled_ext_path = root / (mod_id + ".disabled");

    fs::path current_path;
    if (fs::exists(normal_path, ec)) {
        current_path = normal_path;
    } else if (fs::exists(disabled_ext_path, ec)) {
        current_path = disabled_ext_path;
    } else {
        return false;
    }

    if (enable) {
        if (current_path == disabled_ext_path) {
            fs::rename(disabled_ext_path, normal_path, ec);
            if (ec) return false;
            current_path = normal_path;
        }
        fs::path marker = current_path / DISABLED_MARKER;
        if (fs::exists(marker, ec)) {
            fs::remove(marker, ec);
        }
        return true;
    } else {
        fs::path marker = current_path / DISABLED_MARKER;
        std::ofstream f(marker);
        return f.good();
    }
}

bool toggle_mod(const std::string& mod_id, const std::string& mods_root) {
    bool current = is_mod_enabled(mod_id, mods_root);
    return set_mod_enabled(mod_id, !current, mods_root);
}

size_t get_active_mod_count(const std::string& mods_root) {
    size_t count = 0;
    auto mods = list_mods(mods_root);
    for (const auto& m : mods) {
        if (m.enabled) count++;
    }
    return count;
}

std::vector<std::string> get_active_mod_roots(const std::string& mods_root) {
    std::vector<std::string> roots;
    auto mods = list_mods(mods_root);
    for (const auto& m : mods) {
        if (m.enabled && m.has_romfs) {
            roots.push_back(m.path);
        }
    }
    return roots;
}

std::vector<ModInfo> get_translation_mods(const std::string& mods_root) {
    std::vector<ModInfo> trans;
    auto mods = list_mods(mods_root);
    for (const auto& m : mods) {
        if (m.category == ModCategory::Translation) {
            trans.push_back(m);
        }
    }
    return trans;
}

bool has_active_translation_mod(const std::string& mods_root) {
    auto trans = get_translation_mods(mods_root);
    for (const auto& m : trans) {
        if (m.enabled) return true;
    }
    return false;
}

const char* category_to_string(ModCategory category) {
    switch (category) {
        case ModCategory::Translation: return "Translation";
        case ModCategory::Graphics:    return "Graphics";
        case ModCategory::Audio:       return "Audio";
        case ModCategory::UI:          return "UI";
        case ModCategory::Gameplay:    return "Gameplay";
        default:                       return "Unknown";
    }
}

extern "C" {

int ac_mods_is_enabled(const char* mod_id) {
    if (!mod_id) return 0;
    return is_mod_enabled(mod_id) ? 1 : 0;
}

int ac_mods_set_enabled(const char* mod_id, int enable) {
    if (!mod_id) return 0;
    return set_mod_enabled(mod_id, enable != 0) ? 1 : 0;
}

int ac_mods_count(void) {
    return static_cast<int>(list_mods().size());
}

} // extern "C"

} // namespace ac::mods
