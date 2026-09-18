/*
 * chrome.h — the shared Cardputer deck header, transport strip and notices.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "ocp/ocp_client.h"

namespace ui {

enum class IndicatorState : uint8_t { Off, Ready, Pending, Active, Fault };

struct ChromeState {
    std::string title;
    ocp::LinkState link = ocp::LinkState::Disconnected;
    IndicatorState gnss = IndicatorState::Off;
    IndicatorState storage = IndicatorState::Off;
    IndicatorState rf = IndicatorState::Off;
    int battery_pct = -1;
    bool charging = false;
    bool live = false;
    bool cached = false;
    bool debug = false;
    std::string transport;
    std::string notice;
};

void beginChrome(const ChromeState &state);
void endChrome(const ChromeState &state);

}  // namespace ui
