/*
 * info_view.cpp — see info_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/info_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "storage/sd_storage.h"
#include "ui/canvas.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kHeapPanelHeight = 58;
constexpr int kHeapSegmentCount = 10;
constexpr int kHeapSegmentWidth = 8;
constexpr int kHeapSegmentGap = 2;

uint16_t heapColour(uint32_t free_bytes, uint32_t total_bytes)
{
    if (!total_bytes) return kMutedSlate;
    uint32_t pct = free_bytes >= total_bytes ? 100 : (free_bytes * 100U) / total_bytes;
    if (pct <= 20) return kFaultRed;
    if (pct <= 40) return kCalibrationYellow;
    return kFieldGreen;
}

void drawHeapPanel(M5Canvas &d, int x, int y, int width, const char *label,
                   bool available, uint32_t free_bytes, uint32_t total_bytes)
{
    d.fillRect(x, y, width, kHeapPanelHeight, kPanelDark);
    d.drawRect(x, y, width, kHeapPanelHeight, kLineBorder);

    d.setTextSize(1);
    d.setCursor(x + 5, y + 4);
    d.setTextColor(kMutedSlate, kPanelDark);
    d.print(label);

    d.setTextSize(2);
    d.setCursor(x + 5, y + 15);
    if (!available) {
        d.setTextColor(kMutedSlate, kPanelDark);
        d.print("--");
    } else {
        d.setTextColor(heapColour(free_bytes, total_bytes), kPanelDark);
        d.printf("%lu KB", (unsigned long)(free_bytes / 1024));
    }

    d.setTextSize(1);
    d.setCursor(x + 5, y + 37);
    d.setTextColor(available ? kMutedSlate : kCalibrationYellow, kPanelDark);
    d.print(!available ? "WAITING" : "FREE");

    if (available && total_bytes) {
        uint32_t pct = free_bytes >= total_bytes ? 100 : (free_bytes * 100U) / total_bytes;
        d.setCursor(x + width - 35, y + 37);
        d.setTextColor(heapColour(free_bytes, total_bytes), kPanelDark);
        d.printf("%3u%%", (unsigned)pct);
    } else if (available) {
        d.setCursor(x + width - 25, y + 37);
        d.setTextColor(kMutedSlate, kPanelDark);
        d.print("--%");
    }

    const int bar_x = x + 5;
    const int bar_y = y + 47;
    const uint16_t fill = heapColour(free_bytes, total_bytes);
    int filled = 0;
    if (available && total_bytes) {
        uint32_t pct = free_bytes >= total_bytes ? 100 : (free_bytes * 100U) / total_bytes;
        filled = static_cast<int>((pct * kHeapSegmentCount + 99U) / 100U);
    }
    for (int i = 0; i < kHeapSegmentCount; ++i) {
        d.fillRect(bar_x + i * (kHeapSegmentWidth + kHeapSegmentGap), bar_y,
                   kHeapSegmentWidth, 6, i < filled ? fill : kTrackDark);
    }
}

void formatDuration(char *out, size_t size, uint64_t ms)
{
    uint64_t s = ms / 1000;
    std::snprintf(out, size, "%llud %lluh %llum", s / 86400, (s % 86400) / 3600,
                  (s % 3600) / 60);
}

void drawMetric(M5Canvas &d, int x, int y, const char *label, const char *value, uint16_t color)
{
    d.setCursor(x, y);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print(label);
    d.setCursor(x + 48, y);
    d.setTextColor(color, kVoidInk);
    d.print(value);
}

}  // namespace

void drawInfoView(const ocp::Client &client, bool probe_status_valid, uint32_t probe_heap,
                  uint32_t probe_heap_total, uint64_t probe_uptime_ms, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const uint32_t deck_heap = ESP.getFreeHeap();
    const uint32_t deck_heap_total = ESP.getHeapSize();
    const bool ready = client.state() == ocp::LinkState::Ready;
    const bool probe_available = ready && probe_status_valid;

    drawHeapPanel(d, 4, kBodyTop + 3, 112, "DECK HEAP", true,
                  deck_heap, deck_heap_total);
    drawHeapPanel(d, 124, kBodyTop + 3, 112, "PROBE HEAP", probe_available,
                  probe_heap, probe_heap_total);

    char deck_uptime[24];
    char probe_uptime[24];
    formatDuration(deck_uptime, sizeof deck_uptime, millis());
    formatDuration(probe_uptime, sizeof probe_uptime, probe_uptime_ms);
    drawMetric(d, 4, kBodyTop + 67, "DECK UP", deck_uptime, kMutedSlate);
    drawMetric(d, 124, kBodyTop + 67, "PROBE UP", probe_available ? probe_uptime : "--",
               probe_available ? kMutedSlate : kCalibrationYellow);

    drawMetric(d, 4, kBodyTop + 83, "LINK", ocp::linkStateName(client.state()),
               ready ? kFieldGreen : kFaultRed);
    drawMetric(d, 124, kBodyTop + 83, "SD", storage::ready() ? "READY" : "ABSENT",
               storage::ready() ? kFieldGreen : kFaultRed);

    endChrome(chrome);
}

}  // namespace ui
