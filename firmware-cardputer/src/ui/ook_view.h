/*
 * ook_view.h — the 433 MHz OOK RX screen (OCP-SPEC §15, D-20): one row per
 * transmitter heard (Princeton remotes, LaCrosse sensors), newest first,
 * with a detail line for the selected one and the probe's health on the
 * hint row when something is wrong.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "model/ook_model.h"
#include "ui/chrome.h"

namespace ui {

/* now_ms is the deck clock the model's received_ms values came from, so
 * rows can say how long ago each transmitter was heard. */
void drawOokView(const model::OokModel &ook, size_t cursor, uint32_t now_ms, const ChromeState &chrome);

}  // namespace ui
