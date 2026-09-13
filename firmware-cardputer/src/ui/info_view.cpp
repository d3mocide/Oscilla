/*
 * info_view.cpp — see info_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/info_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"

namespace ui {

namespace {

constexpr int kLineH = 10;

void label(M5Canvas &d, const char *text)
{
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.printf("  %-7s", text);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
}

void printDuration(M5Canvas &d, uint64_t ms)
{
    uint64_t s = ms / 1000;
    d.printf("%llud %lluh %llum\n", s / 86400, (s % 86400) / 3600, (s % 3600) / 60);
}

}  // namespace

void drawInfoView(const ocp::Client &client, bool probe_status_valid, uint32_t probe_heap,
                  uint64_t probe_uptime_ms, uint32_t status_age_ms, const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.println("INFO");

    d.setTextColor(TFT_MAGENTA, TFT_BLACK);
    d.println("DECK");
    label(d, "heap");
    d.printf("%lu KB free\n", (unsigned long)(ESP.getFreeHeap() / 1024));

    label(d, "batt");
    int32_t pct = M5.Power.getBatteryLevel();
    if (pct < 0) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.println("n/a");
    } else {
        bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging;
        d.setTextColor(pct > 30 ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
        d.printf("%ld%%%s\n", (long)pct, charging ? " (charging)" : "");
    }

    label(d, "uptime");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    printDuration(d, millis());

    d.println();
    d.setTextColor(TFT_MAGENTA, TFT_BLACK);
    d.print("PROBE   ");
    bool ready = client.state() == ocp::LinkState::Ready;
    d.setTextColor(ready ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    d.println(ocp::linkStateName(client.state()));

    if (!ready) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.println("  no probe");
    } else if (!probe_status_valid) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.println("  waiting for status...");
    } else {
        label(d, "heap");
        d.printf("%lu KB free\n", (unsigned long)(probe_heap / 1024));
        label(d, "uptime");
        printDuration(d, probe_uptime_ms);
        label(d, "updated");
        d.printf("%lus ago\n", (unsigned long)(status_age_ms / 1000));
    }

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(", / cards");
}

}  // namespace ui
