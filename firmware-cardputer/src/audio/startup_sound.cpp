#include "audio/startup_sound.h"

#include <cstddef>

namespace audio {
namespace {

constexpr int8_t kSine[] = {
    0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126,
    127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12,
    0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126,
    -127, -126, -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12,
};

struct Voice {
    uint16_t start_ms, duration_ms, attack_ms, hz;
    uint8_t level;
    bool bell;
};

// D minor add-nine pad, answered by a glassy rising motif.
constexpr Voice kVoices[] = {
    {0, 1470, 340, 294, 35, false},
    {100, 1440, 360, 440, 17, false},
    {210, 1450, 420, 349, 15, false},
    {350, 530, 30, 587, 26, true},
    {535, 490, 24, 698, 25, true},
    {710, 590, 30, 880, 27, true},
    {920, 780, 45, 1319, 27, true},
    {1070, 730, 95, 1175, 20, true},
    {1170, 630, 65, 587, 15, false},
};

constexpr uint32_t samples(uint32_t ms)
{
    return ms * kStartupSampleRate / 1000;
}

constexpr int wave(uint32_t phase)
{
    const unsigned index = (phase >> 10) & 63;
    const int fraction = phase & 1023;
    return kSine[index] + (kSine[(index + 1) & 63] - kSine[index]) * fraction / 1024;
}

constexpr auto render()
{
    std::array<int16_t, samples(kStartupDurationMs)> mix{};
    for (const auto &voice : kVoices) {
        const uint32_t start = samples(voice.start_ms);
        const uint32_t duration = samples(voice.duration_ms);
        const uint32_t attack = samples(voice.attack_ms);
        const uint32_t step = voice.hz * 65536u / kStartupSampleRate;

        for (uint32_t i = 0; i < duration; ++i) {
            const int envelope = i < attack ? i * 256 / attack
                                             : (duration - i) * 256 / (duration - attack);
            const uint32_t phase = i * step;
            const int overtone = voice.bell ? wave(phase * 3 + phase / 64) : wave(phase * 2);
            const int sound = (wave(phase) * 4 + overtone) / 5;
            mix[start + i] += sound * envelope * voice.level / (127 * 256);
        }
    }

    std::array<int8_t, samples(kStartupDurationMs)> pcm{};
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        const int value = mix[i];
        pcm[i] = value < -127 ? -127 : value > 127 ? 127 : value;
    }
    return pcm;
}

constexpr auto kPcm = render();
static_assert(kPcm.front() == 0 && kPcm.back() == 0, "Startup clip must start and end at silence");

}  // namespace

const decltype(kStartupSound) kStartupSound = kPcm;

}  // namespace audio
