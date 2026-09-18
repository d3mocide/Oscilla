/*
 * deck_navigation.h — the deck's grouped route order and labels.
 *
 * The brand guide owns the information architecture; this module keeps the
 * route metadata out of DeckApp's command/state orchestration.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace app {

enum class Screen : uint8_t {
    Link,
    Sweep,
    Trace,
    Contacts,
    Info,
    Spectrum,
    Mesh,
    SubGhz,
    Deauth,
    AntiSurveillance,
    Drive,
    Beacons,
    Settings,
};

enum class Section : uint8_t { System, Observe, Analyze, Drive, Logs };

struct NavItem {
    Screen screen;
    Section section;
    const char *title;
};

const NavItem &navItem(Screen screen);
bool isHomeCard(Screen screen);
Screen cycleScreen(Screen screen, int direction);

}  // namespace app
