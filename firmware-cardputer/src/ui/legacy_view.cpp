/*
 * legacy_view.cpp — see legacy_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/legacy_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

namespace {
void formatFreqMhz(uint32_t freq_hz, char *buf, size_t n)
{
    std::snprintf(buf, n, "%lu.%03lu",
                  (unsigned long)(freq_hz / 1000000UL),
                  (unsigned long)((freq_hz % 1000000UL) / 1000UL));
}
}  // namespace

void drawLegacyView(const model::LegacyModel &legacy, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    char freq[16] = "";
    if (legacy.hasConfig()) formatFreqMhz(legacy.freqHz(), freq, sizeof freq);

    /* "CHUNKS", not "PACKETS": no sync word, no CRC — these are raw
     * carrier-sense-gated FIFO drains, not decoded/framed packets. See
     * model/legacy_model.h. */
    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("CHUNKS %u  %s", (unsigned)legacy.total(), legacy.active() ? "LISTENING" : "IDLE");
    if (legacy.hasConfig()) {
        d.setCursor(164, kBodyTop + 2);
        d.print(freq);
    } else {
        d.setCursor(164, kBodyTop + 2);
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("UNCONFIGURED");
    }

    const auto &chunks = legacy.chunks();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + static_cast<size_t>(i) < chunks.size(); i++) {
        const auto &c = chunks[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        d.setCursor(6, y);
        d.setTextColor(rssiColour(c.rssi), bg);
        d.printf("%4d", c.rssi);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" %3u ", c.len);
        d.setTextColor(kDimGreen, bg);
        d.print(printable(c.hex, 26).c_str());
    }

    if (chunks.empty()) {
        d.setTextColor(kDimGreen, kVoidInk);
        d.setCursor(0, kListTop);
        d.print(legacy.active() ? "LISTENING FOR CC1101 RX..."
                                 : legacy.hasConfig() ? "PRESS s TO START" : "PRESS c TO CONFIGURE");
    }

    d.setCursor(4, kDetailRowY);
    if (legacy.hasConfig()) {
        d.setTextColor(legacy.active() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("%s N%u", freq, (unsigned)legacy.total());
        if (legacy.fifoOverflows() || legacy.queueDrops() || legacy.malformedCount()) {
            d.setTextColor(kSignalPink, kVoidInk);
            d.printf(" OVF%u QD%u BAD%u", (unsigned)legacy.fifoOverflows(),
                     (unsigned)legacy.queueDrops(), (unsigned)legacy.malformedCount());
        }
    } else {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("CFG REQUIRED · PRESS c");
    }

    endChrome(chrome);
}

}  // namespace ui
