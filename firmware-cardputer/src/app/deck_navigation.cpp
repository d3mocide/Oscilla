/*
 * deck_navigation.cpp — see deck_navigation.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/deck_navigation.h"

namespace app {

namespace {

constexpr NavItem kCards[] = {
    {Screen::Link,     Section::System,  "LINK"},
    {Screen::Info,     Section::System,  "SYSTEM"},
    {Screen::Sweep,    Section::Observe, "WIFI SCAN"},
    {Screen::Beacons,  Section::Observe, "BLE SCAN"},
    {Screen::Mesh,     Section::Observe, "802.15.4"},
    {Screen::SubGhz,   Section::Observe, "LORA RX"},
    {Screen::Spectrum, Section::Analyze, "PACKET MONITOR"},
    {Screen::Contacts, Section::Analyze, "SNIFFER"},
    {Screen::Drive,    Section::Drive,   "WARDRIVE"},
};

constexpr int kNumCards = sizeof(kCards) / sizeof(kCards[0]);

}  // namespace

const NavItem &navItem(Screen screen)
{
    for (const NavItem &item : kCards) {
        if (item.screen == screen) return item;
    }

    static constexpr NavItem kTrace = {Screen::Trace, Section::Observe, "AP DETAIL"};
    static constexpr NavItem kDeauth = {Screen::Deauth, Section::Analyze, "DEAUTH DETECT"};
    return screen == Screen::Trace ? kTrace : kDeauth;
}

bool isHomeCard(Screen screen)
{
    for (const NavItem &item : kCards) {
        if (item.screen == screen) return true;
    }
    return false;
}

Screen cycleScreen(Screen screen, int direction)
{
    int index = 0;
    for (; index < kNumCards; ++index) {
        if (kCards[index].screen == screen) break;
    }
    if (index == kNumCards) index = 0;
    index = (index + direction + kNumCards) % kNumCards;
    return kCards[index].screen;
}

}  // namespace app
