/*
 * settings_view.h — the System Settings card: on-device flags persisted to
 * the SD card (storage::settings, DESIGN §7.6). Display brightness today,
 * more later, same one-flag-file-per-setting shape as debug mode.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "ui/chrome.h"

namespace ui {

void drawSettingsView(uint8_t brightness, const ChromeState &chrome);

}  // namespace ui
