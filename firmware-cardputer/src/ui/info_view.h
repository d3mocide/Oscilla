/*
 * info_view.h — the Info card: deck + probe health (free heap, uptime,
 * deck battery). A diagnostic card, not a DESIGN §7.2 survey view.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "ocp/ocp_client.h"

namespace ui {

/* Deck-local readings (heap/battery/uptime) are read directly from M5Unified
 * inside the view, same as other views touch M5Cardputer.Display directly
 * (DESIGN §7.1: the view layer is where framework-specific code belongs).
 * The probe's numbers arrive over OCP, so DeckApp owns and passes them in. */
void drawInfoView(const ocp::Client &client, bool probe_status_valid, uint32_t probe_heap,
                  uint64_t probe_uptime_ms, uint32_t status_age_ms, const std::string &notice);

}  // namespace ui
