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
constexpr uintptr_t SLOT_TEXT_LANG_INDEX = 0xdc26f8ull;
constexpr uintptr_t SLOT_TEXT_LANG_DESIRED = 0xdc2750ull;
constexpr uintptr_t GET_TEXT_LANGUAGE_FN = 0x9c2e70ull;

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

// Actor Combat Status & Health offsets (Directly inside PlayerActor)
constexpr uintptr_t OFFSET_ACTOR_DIRECT_HP        = 0x840ull;
constexpr uintptr_t OFFSET_ACTOR_DIRECT_MAX_HP    = 0x844ull;
constexpr uintptr_t OFFSET_ACTOR_LEGION_ENERGY    = 0xd54ull;
constexpr uintptr_t OFFSET_ACTOR_LEGION_MAX_ENERGY= 0xd58ull;
constexpr uintptr_t OFFSET_ACTOR_LEGION_EXTRA     = 0xd5cull;

constexpr uintptr_t SLOT_HUD_LEGION               = 0xdb8580ull;
constexpr uintptr_t OFFSET_HUD_LEGION_RATIO       = 0xb0ull;

constexpr int32_t DEFAULT_PLAYER_MAX_HP           = 1000;
constexpr float   DEFAULT_LEGION_MAX_ENERGY       = 100.0f;

enum class VoiceLanguage : int32_t {
    Auto     = -1,
    Japanese = 0,
    English  = 1
};

enum class TextLanguage : int32_t {
    Auto                 = -1,
    Japanese             = 0,
    AmericanEnglish      = 1,
    French               = 2,
    German               = 3,
    Italian              = 4,
    Spanish              = 5,
    SimplifiedChinese    = 6,
    TraditionalChinese   = 7,
    Korean               = 8,
    Dutch                = 9,
    Portuguese           = 10,
    Russian              = 11,
    Taiwanese            = 12,
    BritishEnglish       = 13,
    LatinAmericanSpanish = 14
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

TextLanguage get_text_language();
bool set_text_language(TextLanguage lang);
bool is_translation_mod_active_for_language(TextLanguage lang);

// Player Actor & World Transform queries
uintptr_t get_player_actor(uint32_t index = 0);
Vec3 get_player_position(uint32_t index = 0);
Vec3 get_player_rotation(uint32_t index = 0);
bool set_player_position(const Vec3& pos, uint32_t index = 0);
bool set_player_rotation(const Vec3& rot, uint32_t index = 0);

// Combat, Health & Energy queries
int32_t get_player_hp(uint32_t index = 0);
int32_t get_player_max_hp(uint32_t index = 0);
bool set_player_hp(int32_t hp, uint32_t index = 0);

float get_legion_energy(uint32_t index = 0);
float get_legion_max_energy(uint32_t index = 0);
float get_legion_energy_raw(uint32_t index = 0);
bool set_legion_energy(float energy, uint32_t index = 0);

} // namespace ac::game_state
