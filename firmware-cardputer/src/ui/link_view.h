/*
 * link_view.h — the Link card: connection health, probe identity, and checks.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

#include "ocp/ocp_client.h"
#include "ui/chrome.h"
#include "ui/display_text.h"

namespace ui {

constexpr size_t kLinkRowCount = 4;

void drawLinkView(const ocp::Client &client, size_t cursor, const ChromeState &chrome);

}  // namespace ui
