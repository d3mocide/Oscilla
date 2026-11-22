/*
 * settings_state.h — shared snapshot & row index for the paired onboard and
 * external Settings views (DESIGN §7.1, Observatory v2).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ui {

enum class SettingsRow : uint8_t {
    Brightness = 0,
    IdleSleep = 1,
    WifiRegion = 2,
    WifiBand = 3,
    LoraProfile = 4,
    AudioCues = 5,
    DebugSerial = 6,
    Count = 7,
};

constexpr size_t kSettingsRowCount = static_cast<size_t>(SettingsRow::Count);

inline const char *settingsRowName(SettingsRow row)
{
    switch (row) {
    case SettingsRow::Brightness:  return "BRIGHTNESS";
    case SettingsRow::IdleSleep:   return "IDLE SLEEP";
    case SettingsRow::WifiRegion:  return "WI-FI REGION";
    case SettingsRow::WifiBand:    return "WI-FI BAND";
    case SettingsRow::LoraProfile: return "LORA PROFILE";
    case SettingsRow::AudioCues:   return "AUDIO CUES";
    case SettingsRow::DebugSerial: return "DEBUG SERIAL";
    case SettingsRow::Count:       break;
    }
    return "SETTING";
}

inline size_t stepSettingsCursor(size_t cursor, int delta)
{
    if (delta < 0) return cursor > 0 ? cursor - 1 : 0;
    if (delta > 0) return cursor + 1 < kSettingsRowCount ? cursor + 1 : kSettingsRowCount - 1;
    return cursor < kSettingsRowCount ? cursor : 0;
}

struct SettingsViewState {
    size_t cursor = 0;
    uint8_t brightness = 160;
    uint16_t idle_timeout_s = 0;
    std::string idle_label = "OFF";
    std::string region_code = "US";
    std::string region_channels = "1-11,36-48,149-165";
    std::string region_status = "SYNCED";
    bool region_ok = true;
    std::string band_code = "auto";
    std::string band_label = "AUTO";
    bool band_ok = true;
    std::string lora_label = "MCORE";
    std::string lora_token = "meshcore_us_ca";
    uint32_t lora_freq_hz = 910525000;
    int lora_sf = 7;
    int lora_bw_khz = 62;
    int lora_cr = 1;
    uint8_t lora_sync = 0x12;
    bool lora_from_sd = false;
    size_t lora_index = 0;
    size_t lora_count = 2;
    bool audio_enabled = false;
    bool debug_enabled = false;
    bool sd_ready = false;
};

}  // namespace ui
