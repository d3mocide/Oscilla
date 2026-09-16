/*
 * link_view.h — the link-status screen: probe identity, link state, counters.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

#include "ocp/ocp_client.h"

namespace ui {

/* Probe-supplied text is untrusted: re-escape before display (OCP-SPEC §6). */
std::string printable(const std::string &s, size_t max_len);

void drawLinkView(const ocp::Client &client, const std::string &last_reply,
                  const std::string &notice, bool debug_mode);

}  // namespace ui
