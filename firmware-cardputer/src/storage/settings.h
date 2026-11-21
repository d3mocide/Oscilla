/*
 * settings.h — small flags persisted on the deck's SD card (DESIGN §7.6).
 * One flag file per setting, no parser: the SD card remembers it, not the
 * firmware image, so it survives a reflash. Not host-testable (real SD/SPI
 * I/O), same as sd_storage.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace storage {

/* Debug console (DeckApp::debugEnabled). Read once at boot; written every
 * time DeckApp::toggleDebugMode() runs. False (not just "unreadable") if
 * there's no SD card at all — never assumed on. */
bool loadDebugMode();
void saveDebugMode(bool on);

/* Internal LCD brightness (M5GFX 0-255 scale), DESIGN §7.6. Read once at
 * boot and applied directly in main.cpp; written every time the Settings
 * screen adjusts it. Falls back to kBrightnessDefault (settings.cpp) if
 * there's no SD card or no saved value — never an unreadable/zero screen. */
uint8_t loadBrightness();
void saveBrightness(uint8_t value);

/* Wi-Fi region code (protocol/ocp_wifi_regions.h), pushed to the probe as
 * `set_region`. Returns the raw saved text, empty when there is none; the
 * caller validates it (app::regionOrDefault). */
std::string loadWifiRegion();
void saveWifiRegion(const std::string &code);

/* Wi-Fi band scope ("auto" / "24" / "5", OCP-SPEC §10.9), pushed as
 * `set_band`. Caller validates with app::bandOrDefault(). */
std::string loadWifiBand();
void saveWifiBand(const std::string &code);

}  // namespace storage
