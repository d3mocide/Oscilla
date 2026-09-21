/*
 * info_view.h — the System card: deck + probe health (free heap, uptime,
 * deck battery). A diagnostic card, not a DESIGN §7.2 survey view.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "ocp/ocp_client.h"
#include "ui/chrome.h"

namespace ui {

/* Deck-local readings (heap/uptime) are read directly from M5Unified
 * inside the view, same as other views touch M5Cardputer.Display directly
 * (DESIGN §7.1: the view layer is where framework-specific code belongs).
 * The probe's numbers, including heap_total for the gauge denominator, arrive
 * over OCP, so DeckApp owns and passes them in. */
void drawInfoView(const ocp::Client &client, bool probe_status_valid, uint32_t probe_heap,
                  uint32_t probe_heap_total, uint64_t probe_uptime_ms, const ChromeState &chrome);

}  // namespace ui
