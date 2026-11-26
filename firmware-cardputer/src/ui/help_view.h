/*
 * help_view.h — compact shared keyboard reference.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "app/deck_navigation.h"
#include "ui/chrome.h"

namespace ui {

enum class HelpDetail {
    None, Hunt, BleDossier, ContactDossier, LoraSpectrum, LoraPacket,
    Channel, SessionSummary, SessionRecords, SessionPacket,
};

void drawHelpView(const ChromeState &chrome, app::Screen screen, HelpDetail detail);

}  // namespace ui
