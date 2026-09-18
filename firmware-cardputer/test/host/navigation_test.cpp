/*
 * navigation_test.cpp — grouped deck route order and label contract.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "app/deck_navigation.h"

namespace {

int g_pass;
int g_fail;

void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

}  // namespace

int main()
{
    using app::Screen;

    check(app::isHomeCard(Screen::Link) && app::isHomeCard(Screen::Drive),
          "system and drive routes are home cards");
    check(!app::isHomeCard(Screen::Trace) && app::isHomeCard(Screen::Deauth) &&
          app::isHomeCard(Screen::AntiSurveillance),
          "AP Detail stays a drill-down while defensive analysis routes are home cards");
    check(!app::isHomeCard(Screen::Settings) &&
          std::string(app::navItem(Screen::Settings).title) == "SETTINGS",
          "Settings stays a drill-down (reached only from Link) with its own nav title");
    check(std::string(app::navItem(Screen::Link).title) == "LINK" &&
          std::string(app::navItem(Screen::Info).title) == "SYSTEM" &&
          std::string(app::navItem(Screen::Sweep).title) == "WIFI SCAN" &&
          std::string(app::navItem(Screen::Beacons).title) == "BLE SCAN" &&
          std::string(app::navItem(Screen::Mesh).title) == "802.15.4" &&
          std::string(app::navItem(Screen::SubGhz).title) == "LORA RX" &&
          std::string(app::navItem(Screen::Spectrum).title) == "PACKET MONITOR" &&
          std::string(app::navItem(Screen::Contacts).title) == "SNIFFER" &&
          std::string(app::navItem(Screen::Deauth).title) == "DEAUTH DETECT" &&
          std::string(app::navItem(Screen::AntiSurveillance).title) == "ANTI-SURV",
          "route labels use standard tool names");
    check(app::cycleScreen(Screen::Link, -1) == Screen::Drive &&
          app::cycleScreen(Screen::Drive, 1) == Screen::Link,
          "home route order wraps in both directions");
    check(app::cycleScreen(Screen::Sweep, 1) == Screen::Beacons &&
          app::cycleScreen(Screen::Spectrum, 1) == Screen::Contacts &&
          app::cycleScreen(Screen::Contacts, 1) == Screen::Deauth &&
          app::cycleScreen(Screen::Deauth, 1) == Screen::AntiSurveillance &&
          app::cycleScreen(Screen::AntiSurveillance, 1) == Screen::Drive,
          "sibling views remain grouped by section");

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "navigation test FAILED" : "navigation test OK",
                g_pass, g_fail);
    return g_fail ? 1 : 0;
}
