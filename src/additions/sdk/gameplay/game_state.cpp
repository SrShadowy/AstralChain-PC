#include "game_state.hpp"
#include "../storage/mods.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ac::game_state {

static uintptr_t get_slot_scene() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_SCENE, SLOT_SCENE);
    return s;
}
static uintptr_t get_slot_phase() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_PHASE, SLOT_PHASE);
    return s;
}
static uintptr_t get_slot_area() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_AREA, SLOT_AREA);
    return s;
}
static uintptr_t get_slot_subarea() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_SUBAREA, SLOT_SUBAREA);
    return s;
}
static uintptr_t get_slot_boss_active() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_BOSS_ACTIVE, SLOT_BOSS_ACTIVE);
    return s;
}
static uintptr_t get_slot_player_table() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_PLAYER_TABLE, SLOT_PLAYER_TABLE);
    return s;
}
static uintptr_t get_slot_voice_opt() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_VOICE_OPT, SLOT_VOICE_OPT);
    return s;
}
static uintptr_t get_slot_voice_se() {
    static uintptr_t s = 0;
    if (!s) s = ac::memory::resolve_slot_from_aob(ac::memory::AOB_VOICE_SE, SLOT_VOICE_SE);
    return s;
}

uint32_t get_scene_id() {
    uint32_t scene_id = 0;
    ac::memory::read_deref<uint32_t>(get_slot_scene(), OFFSET_SCENE_ID, &scene_id);
    return scene_id;
}

uint32_t get_phase_id() {
    uint32_t phase_id = 0;
    ac::memory::read_deref<uint32_t>(get_slot_phase(), 0, &phase_id);
    return phase_id;
}

uint32_t get_area_id() {
    uint32_t area = 0;
    ac::memory::read_deref<uint32_t>(get_slot_area(), 0, &area);
    return area;
}

uint32_t get_subarea_id() {
    uint32_t subarea = 0;
    ac::memory::read_deref<uint32_t>(get_slot_subarea(), 0, &subarea);
    return subarea;
}

bool is_boss_active() {
    uint32_t active = 0;
    ac::memory::read_deref<uint32_t>(get_slot_boss_active(), 0, &active);
    return active != 0;
}

uint32_t get_boss_id() {
    uint32_t boss_id = 0;
    ac::memory::read_deref<uint32_t>(SLOT_BOSS_ID, 0, &boss_id);
    return boss_id;
}

LegionType get_active_legion() {
    uintptr_t actor = get_player_actor(0);
    if (actor) {
        int32_t legion_id = -1;
        if (ac::memory::read_raw<int32_t>(actor + OFFSET_PLAYER_LEGION_ACTIVE, &legion_id)) {
            if (legion_id >= 0 && legion_id <= 4) {
                return static_cast<LegionType>(legion_id);
            }
        }
        if (ac::memory::read_raw<int32_t>(actor + OFFSET_PLAYER_LEGION_SELECTED, &legion_id)) {
            if (legion_id >= 0 && legion_id <= 4) {
                return static_cast<LegionType>(legion_id);
            }
        }
    }
    return LegionType::None;
}

VoiceLanguage get_voice_language() {
    int32_t se_val = -1;
    if (ac::memory::read_deref<int32_t>(get_slot_voice_se(), 0, &se_val)) {
        if (se_val == 0 || se_val == 1) {
            return static_cast<VoiceLanguage>(se_val);
        }
    }

    int32_t opt_val = -1;
    if (ac::memory::read_deref<int32_t>(get_slot_voice_opt(), 0, &opt_val)) {
        if (opt_val == 0 || opt_val == 1) {
            return static_cast<VoiceLanguage>(opt_val);
        }
    }
    return VoiceLanguage::Auto;
}

bool set_voice_language(VoiceLanguage lang) {
    if (lang != VoiceLanguage::Japanese && lang != VoiceLanguage::English) {
        return false;
    }

    int32_t val = static_cast<int32_t>(lang);
    if (!ac::memory::write_deref<int32_t>(get_slot_voice_opt(), 0, val)) {
        return false;
    }

    // Trigger Wwise soundbank reloads by signaling dirty flags
    constexpr uint32_t DIRTY_FLAG = 1;
    ac::memory::write_deref<uint32_t>(SLOT_VOICE_DIRTY1, 0, DIRTY_FLAG);
    ac::memory::write_deref<uint32_t>(SLOT_VOICE_DIRTY2, 0, DIRTY_FLAG);
    return true;
}

TextLanguage get_text_language() {
    int32_t val = -1;
    if (ac::memory::read<int32_t>(SLOT_TEXT_LANG_INDEX, &val)) {
        if (val >= 0 && val <= 14) {
            return static_cast<TextLanguage>(val);
        }
    }
    return TextLanguage::Auto;
}

bool set_text_language(TextLanguage lang) {
    if (lang < TextLanguage::Japanese || lang > TextLanguage::LatinAmericanSpanish) {
        return false;
    }
    int32_t val = static_cast<int32_t>(lang);
    if (!ac::memory::write<int32_t>(SLOT_TEXT_LANG_INDEX, val)) {
        return false;
    }
    ac::memory::write<int32_t>(SLOT_TEXT_LANG_DESIRED, val);
    return true;
}

bool is_translation_mod_active_for_language(TextLanguage lang) {
    auto trans_mods = ac::mods::get_translation_mods();
    for (const auto& m : trans_mods) {
        if (!m.enabled) continue;
        if (lang == TextLanguage::Auto) {
            return true;
        }
        std::string tgt = m.target_language;
        std::transform(tgt.begin(), tgt.end(), tgt.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::string id_low = m.id;
        std::transform(id_low.begin(), id_low.end(), id_low.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lang == TextLanguage::Portuguese) {
            if (tgt.find("pt") != std::string::npos || tgt.find("portuguese") != std::string::npos ||
                id_low.find("pt") != std::string::npos || id_low.find("portugues") != std::string::npos) {
                return true;
            }
        } else if (lang == TextLanguage::Spanish || lang == TextLanguage::LatinAmericanSpanish) {
            if (tgt.find("es") != std::string::npos || tgt.find("spanish") != std::string::npos ||
                id_low.find("es") != std::string::npos || id_low.find("spanish") != std::string::npos) {
                return true;
            }
        } else if (lang == TextLanguage::French) {
            if (tgt.find("fr") != std::string::npos || id_low.find("fr") != std::string::npos) return true;
        } else if (lang == TextLanguage::German) {
            if (tgt.find("de") != std::string::npos || id_low.find("de") != std::string::npos) return true;
        } else if (lang == TextLanguage::Italian) {
            if (tgt.find("it") != std::string::npos || id_low.find("it") != std::string::npos) return true;
        } else if (lang == TextLanguage::Russian) {
            if (tgt.find("ru") != std::string::npos || id_low.find("ru") != std::string::npos) return true;
        }
    }
    return false;
}

uintptr_t get_player_actor(uint32_t index) {
    uintptr_t table = 0;
    if (!ac::memory::read<uintptr_t>(get_slot_player_table(), &table) || !table) {
        return 0;
    }
    if ((table & 7) != 0 || !ac::memory::is_valid_guest_ptr(table + index * sizeof(uintptr_t), sizeof(uintptr_t))) {
        return 0;
    }
    uintptr_t actor = 0;
    if (!ac::memory::read_raw<uintptr_t>(table + index * sizeof(uintptr_t), &actor) || !actor) {
        return 0;
    }
    if ((actor & 7) != 0 || !ac::memory::is_valid_guest_ptr(actor, 0x1000)) {
        return 0;
    }
    // Verify vtable pointer inside guest memory to ensure it is a valid instantiated object
    uintptr_t vtable = 0;
    if (!ac::memory::read_raw<uintptr_t>(actor, &vtable) || !vtable || (vtable & 7) != 0 || !ac::memory::is_valid_guest_ptr(vtable, sizeof(uintptr_t))) {
        return 0;
    }
    return actor;
}

Vec3 get_player_position(uint32_t index) {
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return pos;
    }
    ac::memory::read_raw<Vec3>(actor + OFFSET_ACTOR_POS_X, &pos);
    return pos;
}

Vec3 get_player_rotation(uint32_t index) {
    Vec3 rot = {0.0f, 0.0f, 0.0f};
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return rot;
    }
    ac::memory::read_raw<Vec3>(actor + OFFSET_ACTOR_ROT_PITCH, &rot);
    return rot;
}

bool set_player_position(const Vec3& pos, uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor || !ac::memory::is_valid_guest_ptr(actor + OFFSET_ACTOR_POS_X, sizeof(Vec3))) {
        return false;
    }
    void* dst = reinterpret_cast<void*>(actor + OFFSET_ACTOR_POS_X);
    std::memcpy(dst, &pos, sizeof(Vec3));
    return true;
}

bool set_player_rotation(const Vec3& rot, uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor || !ac::memory::is_valid_guest_ptr(actor + OFFSET_ACTOR_ROT_PITCH, sizeof(Vec3))) {
        return false;
    }
    void* dst = reinterpret_cast<void*>(actor + OFFSET_ACTOR_ROT_PITCH);
    std::memcpy(dst, &rot, sizeof(Vec3));
    return true;
}

int32_t get_player_hp(uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return 0;
    }

    int32_t hp = 0;
    if (ac::memory::read_raw<int32_t>(actor + OFFSET_ACTOR_DIRECT_HP, &hp)) {
        return hp;
    }
    return 0;
}

int32_t get_player_max_hp(uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return DEFAULT_PLAYER_MAX_HP;
    }

    int32_t max_hp = 0;
    if (ac::memory::read_raw<int32_t>(actor + OFFSET_ACTOR_DIRECT_MAX_HP, &max_hp) && max_hp > 0) {
        return max_hp;
    }
    return DEFAULT_PLAYER_MAX_HP;
}

bool set_player_hp(int32_t hp, uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor || !ac::memory::is_valid_guest_ptr(actor + OFFSET_ACTOR_DIRECT_HP, sizeof(int32_t))) {
        return false;
    }

    void* direct_dst = reinterpret_cast<void*>(actor + OFFSET_ACTOR_DIRECT_HP);
    std::memcpy(direct_dst, &hp, sizeof(int32_t));
    return true;
}

float get_legion_energy(uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return 0.0f;
    }

    float current = 0.0f;
    float max_nrg = 0.0f;
    if (ac::memory::read_raw<float>(actor + OFFSET_ACTOR_LEGION_ENERGY, &current)) {
        if (ac::memory::read_raw<float>(actor + OFFSET_ACTOR_LEGION_MAX_ENERGY, &max_nrg) && max_nrg > 0.0f) {
            float pct = (current / max_nrg) * 100.0f;
            return std::clamp(pct, 0.0f, 100.0f);
        }
        if (current >= 0.0f && current <= 1.0f) {
            return current * 100.0f;
        }
        return std::clamp(current, 0.0f, DEFAULT_LEGION_MAX_ENERGY);
    }

    // Secondary fallback: read from HUD Legion manager gauge ratio
    float hud_ratio = 0.0f;
    if (ac::memory::read_deref<float>(SLOT_HUD_LEGION, OFFSET_HUD_LEGION_RATIO, &hud_ratio)) {
        return std::clamp(hud_ratio * 100.0f, 0.0f, 100.0f);
    }

    return 0.0f;
}

float get_legion_max_energy(uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return DEFAULT_LEGION_MAX_ENERGY;
    }
    float max_nrg = 0.0f;
    if (ac::memory::read_raw<float>(actor + OFFSET_ACTOR_LEGION_MAX_ENERGY, &max_nrg) && max_nrg > 0.0f) {
        return max_nrg;
    }
    return DEFAULT_LEGION_MAX_ENERGY;
}

float get_legion_energy_raw(uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return 0.0f;
    }
    float current = 0.0f;
    ac::memory::read_raw<float>(actor + OFFSET_ACTOR_LEGION_ENERGY, &current);
    return current;
}

bool set_legion_energy(float energy, uint32_t index) {
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return false;
    }

    float max_nrg = 100.0f;
    ac::memory::read_raw<float>(actor + OFFSET_ACTOR_LEGION_MAX_ENERGY, &max_nrg);
    if (max_nrg <= 0.0f) {
        max_nrg = 100.0f;
    }

    float target_val = energy;
    if (energy <= 100.0f && std::fabs(max_nrg - 100.0f) > 0.001f) {
        target_val = (energy / 100.0f) * max_nrg;
    }
    float clamped_nrg = std::clamp(target_val, 0.0f, max_nrg);

    if (ac::memory::is_valid_guest_ptr(actor + OFFSET_ACTOR_LEGION_ENERGY, sizeof(float))) {
        void* direct_dst = reinterpret_cast<void*>(actor + OFFSET_ACTOR_LEGION_ENERGY);
        std::memcpy(direct_dst, &clamped_nrg, sizeof(float));
        return true;
    }
    return false;
}

} // namespace ac::game_state
