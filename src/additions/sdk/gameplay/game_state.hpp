#pragma once
#include <cstdint>
#include "../core/types.hpp"
#include "../core/memory.hpp"

namespace ac::game_state {

// Core guest pointer slots in .data / .bss
constexpr uintptr_t SLOT_SAVE           = 0xdb83d8ull;
constexpr uintptr_t SLOT_SCENE          = 0xdb84c8ull;
constexpr uintptr_t SLOT_PHASE          = 0xdb84f8ull;
constexpr uintptr_t SLOT_SCENARIO       = 0xdb8930ull;
constexpr uintptr_t SLOT_PLAYER_TABLE   = 0xdbb680ull;
constexpr uintptr_t SLOT_MISSION        = 0xdbc420ull;
constexpr uintptr_t SLOT_BOSS_ACTIVE    = 0xdbc928ull;
constexpr uintptr_t SLOT_BOSS_STAGE     = 0xdbc930ull;
constexpr uintptr_t SLOT_BATTLE_STATE   = 0xdbc940ull;
constexpr uintptr_t SLOT_BATTLE_TIMER   = 0xdbc948ull;
constexpr uintptr_t SLOT_BOSS_ID        = 0xdbc950ull;
constexpr uintptr_t SLOT_AREA           = 0xdbcf10ull;
constexpr uintptr_t SLOT_SUBAREA        = 0xdbcf20ull;
constexpr uintptr_t SLOT_LEGION         = 0xdc5bb8ull;

// Sound & Language configuration slots
constexpr uintptr_t SLOT_VOICE_OPT      = 0xdc13b0ull;
constexpr uintptr_t SLOT_VOICE_DIRTY1   = 0xdc13b8ull;
constexpr uintptr_t SLOT_VOICE_DIRTY2   = 0xdc13c0ull;
constexpr uintptr_t SLOT_VOICE_SE       = 0xdc2908ull;

// Internal struct offsets
constexpr uintptr_t OFFSET_SCENE_ID               = 0x180ull;
constexpr uintptr_t OFFSET_SAVE_FILE              = 0x16ull;
constexpr uintptr_t OFFSET_SAVE_DIFF              = 0xaefa0ull;
constexpr uintptr_t OFFSET_PLAYER_LEGION_ACTIVE   = 0xf48ull;
constexpr uintptr_t OFFSET_PLAYER_LEGION_SELECTED = 0xf38ull;

// Actor & Player Transform offsets (Platinum Actor base class)
constexpr uintptr_t OFFSET_ACTOR_POS_X            = 0x50ull;
constexpr uintptr_t OFFSET_ACTOR_POS_Y            = 0x54ull;
constexpr uintptr_t OFFSET_ACTOR_POS_Z            = 0x58ull;
constexpr uintptr_t OFFSET_ACTOR_ROT_PITCH        = 0x90ull;
constexpr uintptr_t OFFSET_ACTOR_ROT_YAW          = 0x94ull;
constexpr uintptr_t OFFSET_ACTOR_ROT_ROLL         = 0x98ull;

enum class VoiceLanguage : int32_t {
    Auto     = -1,
    Japanese = 0,
    English  = 1
};

enum class Difficulty : int32_t {
    SuperCasual = -2,
    Casual      = 0,
    Standard    = 1,
    Hardcore    = 2
};

enum class LegionType : int32_t {
    Sword = 0,
    Arrow = 1,
    Arm   = 2,
    Beast = 3,
    Axe   = 4,
    None  = -1
};

uint32_t get_scene_id();
uint32_t get_phase_id();
uint32_t get_area_id();
uint32_t get_subarea_id();
bool is_boss_active();
uint32_t get_boss_id();
LegionType get_active_legion();
VoiceLanguage get_voice_language();
bool set_voice_language(VoiceLanguage lang);

// Player Actor & World Transform queries
uintptr_t get_player_actor(uint32_t index = 0);
Vec3 get_player_position(uint32_t index = 0);
Vec3 get_player_rotation(uint32_t index = 0);

} // namespace ac::game_state
