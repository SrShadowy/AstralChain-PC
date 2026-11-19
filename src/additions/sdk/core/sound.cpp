#include "sound.hpp"
#include <algorithm>
#include <array>

extern "C" {
__attribute__((weak)) int g_extra_audio_volume = -1;
__attribute__((weak)) int g_extra_audio_mute_unfocus = -1;
__attribute__((weak)) uint32_t g_extra_audio_gen = 0;
}

namespace ac::sound {

namespace {

constexpr size_t CHANNEL_COUNT = 5;
std::array<int32_t, CHANNEL_COUNT> s_channel_volumes = {
    VOLUME_DEFAULT_PCT,
    VOLUME_DEFAULT_PCT,
    VOLUME_DEFAULT_PCT,
    VOLUME_DEFAULT_PCT,
    VOLUME_DEFAULT_PCT
};

} // namespace

void set_volume(AudioChannel channel, int32_t pct) {
    int32_t clamped = std::clamp(pct, VOLUME_MIN_PCT, VOLUME_MAX_PCT);
    size_t idx = static_cast<size_t>(channel);
    if (idx < CHANNEL_COUNT) {
        s_channel_volumes[idx] = clamped;
    }

    if (channel == AudioChannel::Master) {
        g_extra_audio_volume = clamped;
        g_extra_audio_gen++;
    }
}

int32_t get_volume(AudioChannel channel) {
    if (channel == AudioChannel::Master) {
        if (g_extra_audio_volume >= 0) {
            return std::clamp(g_extra_audio_volume, VOLUME_MIN_PCT, VOLUME_MAX_PCT);
        }
    }

    size_t idx = static_cast<size_t>(channel);
    if (idx < CHANNEL_COUNT) {
        return s_channel_volumes[idx];
    }
    return VOLUME_DEFAULT_PCT;
}

void set_master_volume(int32_t pct) {
    set_volume(AudioChannel::Master, pct);
}

int32_t get_master_volume() {
    return get_volume(AudioChannel::Master);
}

void set_bgm_volume(int32_t pct) {
    set_volume(AudioChannel::BGM, pct);
}

int32_t get_bgm_volume() {
    return get_volume(AudioChannel::BGM);
}

void set_sfx_volume(int32_t pct) {
    set_volume(AudioChannel::SFX, pct);
}

int32_t get_sfx_volume() {
    return get_volume(AudioChannel::SFX);
}

void set_voice_volume(int32_t pct) {
    set_volume(AudioChannel::Voice, pct);
}

int32_t get_voice_volume() {
    return get_volume(AudioChannel::Voice);
}

void set_mute_on_unfocus(bool enable) {
    g_extra_audio_mute_unfocus = enable ? 1 : 0;
    g_extra_audio_gen++;
}

bool is_mute_on_unfocus() {
    return g_extra_audio_mute_unfocus > 0;
}

float calculate_effective_gain(int32_t volume_pct, bool has_focus, bool mute_unfocus) {
    if (mute_unfocus && !has_focus) {
        return 0.0f;
    }
    int32_t clamped = std::clamp(volume_pct, VOLUME_MIN_PCT, VOLUME_MAX_PCT);
    return static_cast<float>(clamped) / GAIN_PERCENT_DIV;
}

} // namespace ac::sound
