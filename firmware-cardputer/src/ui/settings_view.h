/*
 * settings_view.h — scrollable System Settings list for the 240x135 deck,
 * paired with drawExternalSettingsStrip on the 320x240 companion display
 * (DESIGN §7.6, Observatory v2).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ui/chrome.h"
#include "ui/settings_state.h"

namespace ui {

void drawSettingsView(const SettingsViewState &state, const ChromeState &chrome);

}  // namespace ui
