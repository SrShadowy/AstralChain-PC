#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac::mods {

constexpr const char* DEFAULT_MODS_DIR = "mods";
constexpr const char* DISABLED_MARKER  = ".disabled";
constexpr const char* MOD_CONFIG_INI   = "mod.ini";

enum class ModCategory : int32_t {
    Unknown = 0,
    Translation = 1,
    Graphics = 2,
    Audio = 3,
    UI = 4,
    Gameplay = 5
};

struct ModInfo {
    std::string id;
    std::string name;
    std::string path;
    std::string description;
    std::string author;
    std::string version;
    std::string target_language;
    ModCategory category;
    bool enabled;
    bool has_romfs;
    bool has_exefs;
    uint64_t total_size_bytes;
};

std::vector<ModInfo> list_mods(const std::string& mods_root = DEFAULT_MODS_DIR);
std::optional<ModInfo> get_mod_info(const std::string& mod_id, const std::string& mods_root = DEFAULT_MODS_DIR);
bool is_mod_enabled(const std::string& mod_id, const std::string& mods_root = DEFAULT_MODS_DIR);
bool set_mod_enabled(const std::string& mod_id, bool enable, const std::string& mods_root = DEFAULT_MODS_DIR);
bool toggle_mod(const std::string& mod_id, const std::string& mods_root = DEFAULT_MODS_DIR);
size_t get_active_mod_count(const std::string& mods_root = DEFAULT_MODS_DIR);
std::vector<std::string> get_active_mod_roots(const std::string& mods_root = DEFAULT_MODS_DIR);
std::vector<ModInfo> get_translation_mods(const std::string& mods_root = DEFAULT_MODS_DIR);
bool has_active_translation_mod(const std::string& mods_root = DEFAULT_MODS_DIR);

const char* category_to_string(ModCategory category);

extern "C" {
int ac_mods_is_enabled(const char* mod_id);
int ac_mods_set_enabled(const char* mod_id, int enable);
int ac_mods_count(void);
}

} // namespace ac::mods
