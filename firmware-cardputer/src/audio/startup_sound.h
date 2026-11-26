// Original Oscilla startup sound, stored as mono signed PCM.
#pragma once

#include <array>
#include <cstdint>

namespace audio {

constexpr uint32_t kStartupSampleRate = 16000;
constexpr uint32_t kStartupDurationMs = 1800;
extern const std::array<int8_t, kStartupSampleRate * kStartupDurationMs / 1000> kStartupSound;

}  // namespace audio
