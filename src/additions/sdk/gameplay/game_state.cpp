#include "game_state.hpp"
#include <cstring>

namespace ac::game_state {

uint32_t get_scene_id() {
    uint32_t scene_id = 0;
    ac::memory::read_deref<uint32_t>(SLOT_SCENE, OFFSET_SCENE_ID, &scene_id);
    return scene_id;
}

uint32_t get_phase_id() {
    uint32_t phase_id = 0;
    ac::memory::read_deref<uint32_t>(SLOT_PHASE, 0, &phase_id);
    return phase_id;
}

uint32_t get_area_id() {
    uint32_t area = 0;
    ac::memory::read_deref<uint32_t>(SLOT_AREA, 0, &area);
    return area;
}

uint32_t get_subarea_id() {
    uint32_t subarea = 0;
    ac::memory::read_deref<uint32_t>(SLOT_SUBAREA, 0, &subarea);
    return subarea;
}

bool is_boss_active() {
    uint32_t active = 0;
    ac::memory::read_deref<uint32_t>(SLOT_BOSS_ACTIVE, 0, &active);
    return active != 0;
}

uint32_t get_boss_id() {
    uint32_t boss_id = 0;
    ac::memory::read_deref<uint32_t>(SLOT_BOSS_ID, 0, &boss_id);
    return boss_id;
}

LegionType get_active_legion() {
    int32_t legion_id = -1;
    if (ac::memory::read_deref<int32_t>(SLOT_PLAYER_TABLE, OFFSET_PLAYER_LEGION_ACTIVE, &legion_id)) {
        if (legion_id >= 0 && legion_id <= 4) {
            return static_cast<LegionType>(legion_id);
        }
    }
    return LegionType::None;
}

VoiceLanguage get_voice_language() {
    int32_t se_val = -1;
    if (ac::memory::read_deref<int32_t>(SLOT_VOICE_SE, 0, &se_val)) {
        if (se_val == 0 || se_val == 1) {
            return static_cast<VoiceLanguage>(se_val);
        }
    }

    int32_t opt_val = -1;
    if (ac::memory::read_deref<int32_t>(SLOT_VOICE_OPT, 0, &opt_val)) {
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
    if (!ac::memory::write_deref<int32_t>(SLOT_VOICE_OPT, 0, val)) {
        return false;
    }

    // Trigger Wwise soundbank reloads by signaling dirty flags
    constexpr uint32_t DIRTY_FLAG = 1;
    ac::memory::write_deref<uint32_t>(SLOT_VOICE_DIRTY1, 0, DIRTY_FLAG);
    ac::memory::write_deref<uint32_t>(SLOT_VOICE_DIRTY2, 0, DIRTY_FLAG);
    return true;
}

uintptr_t get_player_actor(uint32_t index) {
    uintptr_t table = 0;
    if (!ac::memory::read<uintptr_t>(SLOT_PLAYER_TABLE, &table) || !table) {
        return 0;
    }
    uintptr_t actor = 0;
    const void* src = reinterpret_cast<const void*>(table + index * sizeof(uintptr_t));
    std::memcpy(&actor, src, sizeof(uintptr_t));
    return actor;
}

Vec3 get_player_position(uint32_t index) {
    Vec3 pos = {0.0f, 0.0f, 0.0f};
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return pos;
    }
    const void* src = reinterpret_cast<const void*>(actor + OFFSET_ACTOR_POS_X);
    std::memcpy(&pos, src, sizeof(Vec3));
    return pos;
}

Vec3 get_player_rotation(uint32_t index) {
    Vec3 rot = {0.0f, 0.0f, 0.0f};
    uintptr_t actor = get_player_actor(index);
    if (!actor) {
        return rot;
    }
    const void* src = reinterpret_cast<const void*>(actor + OFFSET_ACTOR_ROT_PITCH);
    std::memcpy(&rot, src, sizeof(Vec3));
    return rot;
}

} // namespace ac::game_state
