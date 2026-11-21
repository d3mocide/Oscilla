/* Rendered reference: master brand guide, Analyze / Packet Monitor deck. */
#include "ui/spectrum_view.h"
#include <M5Cardputer.h>
#include <algorithm>
#include "ui/canvas.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {
void drawSpectrumView(const model::SpectrumModel &s, size_t cursor, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    const auto &readings = s.readings();
    if (s.locked()) cursor = 0;
    const uint32_t now = millis();
    uint32_t scale = 1;
    for (const auto &r : readings) scale = std::max(scale, r.pkts);
    d.setTextSize(1);
    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.setCursor(5, kBodyTop + 3);
    d.print(s.locked() ? "LOCKED / 1s WINDOW" : "CHANNEL FIELD / DWELL");
    d.drawFastHLine(5, kBodyTop + 14, 230, kDimGreen);
    const size_t start = listFirstVisible(cursor, kListVisibleRows);
    for (size_t row = 0; row < static_cast<size_t>(kListVisibleRows) && start + row < readings.size(); ++row) {
        const size_t index = start + row;
        const auto &r = readings[index];
        const int y = kBodyTop + 20 + static_cast<int>(row) * kListRowHeight;
        const bool sel = index == cursor;
        const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        const uint16_t colour = sel ? kSignalPink : kPaperPhosphor;
        drawRowHighlight(d, y, sel);
        d.setTextColor(colour, bg);
        d.setCursor(6, y);
        if (r.valid) d.printf("CH %-3u %lu", r.ch, (unsigned long)r.pkts);
        else d.printf("CH %-3u --", r.ch);
        const uint32_t age = now - r.received_ms;
        d.setCursor(165, y);
        d.setTextColor(!s.active() || age >= 3000 ? kMutedSlate : colour, bg);
        if (!r.valid) d.print("WAIT");
        else d.printf("%lus", (unsigned long)(age / 1000));
        d.fillRect(201, y + 2, 28, 4, kTrackDark);
        if (r.valid && r.pkts) {
            d.fillRect(201, y + 2, static_cast<uint64_t>(r.pkts) * 28 / scale, 4,
                       sel ? kSignalPink : kFieldGreen);
        }
    }
    if (readings.empty()) {
        d.setCursor(8, kBodyTop + 40);
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(s.active() ? "WAITING FOR CHANNEL DATA" : "NO CHANNEL DATA / START s");
    }
    d.drawFastHLine(5, kBodyTop + 86, 230, kDimGreen);
    d.setCursor(5, kHintRowY);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print(s.locked() ? "` CHANNELS / s STOP-RESUME" : "ENTER LOCK / ; . FOCUS");
    endChrome(chrome);
}
}
