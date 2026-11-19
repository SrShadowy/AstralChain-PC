#pragma once
#include <cstdint>

namespace ac::sound {

// Wwise RTPC volume parameter names
constexpr const char* WWISE_PARAM_MASTER = "Volume_Master";
constexpr const char* WWISE_PARAM_BGM    = "Volume_BGM";
constexpr const char* WWISE_PARAM_SFX    = "Volume_SE";
constexpr const char* WWISE_PARAM_VOICE  = "Volume_Voice";
constexpr const char* WWISE_PARAM_ENV    = "Volume_Environment";

// Nintendo Switch / Audren route endpoints
constexpr const char* DEVICE_STEREO_JACK = "AudioStereoJackOutput";
constexpr const char* DEVICE_SPEAKER     = "AudioBuiltInSpeakerOutput";
constexpr const char* DEVICE_TV          = "AudioTvOutput";
constexpr const char* DEVICE_USB         = "AudioUsbDeviceOutput";
constexpr const char* DEVICE_EXTERNAL    = "AudioExternalOutput";

constexpr int32_t VOLUME_MIN_PCT     = 0;
constexpr int32_t VOLUME_MAX_PCT     = 100;
constexpr int32_t VOLUME_DEFAULT_PCT = 100;
constexpr float   GAIN_PERCENT_DIV   = 100.0f;

enum class AudioChannel : uint32_t {
    Master      = 0,
    BGM         = 1,
    SFX         = 2,
    Voice       = 3,
    Environment = 4
};

// Real-time volume controls (0..100 %)
void set_volume(AudioChannel channel, int32_t pct);
int32_t get_volume(AudioChannel channel);

void set_master_volume(int32_t pct);
int32_t get_master_volume();

void set_bgm_volume(int32_t pct);
int32_t get_bgm_volume();

void set_sfx_volume(int32_t pct);
int32_t get_sfx_volume();

void set_voice_volume(int32_t pct);
int32_t get_voice_volume();

// Unfocus mute & effective gain
void set_mute_on_unfocus(bool enable);
bool is_mute_on_unfocus();

float calculate_effective_gain(int32_t volume_pct, bool has_focus, bool mute_unfocus);

} // namespace ac::sound
