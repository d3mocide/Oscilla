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

namespace ui {

constexpr size_t kLinkRowCount = 4;

/* Probe-supplied text is untrusted: re-escape before display (OCP-SPEC §6). */
std::string printable(const std::string &s, size_t max_len);

void drawLinkView(const ocp::Client &client, size_t cursor, const ChromeState &chrome);

}  // namespace ui
