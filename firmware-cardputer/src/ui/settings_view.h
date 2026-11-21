/*
 * settings_view.h — the System Settings card: on-device flags persisted to
 * the SD card (storage::settings, DESIGN §7.6): display brightness and the
 * Wi-Fi region (OCP-SPEC §10.8), one flag file per setting.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "ui/chrome.h"

namespace ui {

/* `region_code` e.g. "US"; `band_label` e.g. "AUTO"/"2.4G"/"5G";
 * `idle_label` e.g. "OFF"/"60s"/"5m"; `region_channels` its hop summary;
 * `region_status` short caps text, `region_ok` true when probe matches. */
void drawSettingsView(uint8_t brightness, const char *idle_label, bool idle_enabled,
                      const char *region_code, const char *band_label,
                      bool band_ok, const char *region_channels, const char *region_status,
                      bool region_ok, const ChromeState &chrome);

}  // namespace ui
