/* Rendered reference: master brand guide, Analyze / Packet Monitor deck. */
#include "ui/spectrum_view.h"
#include <M5Cardputer.h>
#include <algorithm>
#include "ui/canvas.h"
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
    for (const auto &r : readings) scale = std::max(scale,r.pkts);
    d.setTextSize(1); d.setTextColor(kPaperPhosphor,kVoidInk);
    d.setCursor(5,kBodyTop+5);
    d.print(s.locked() ? "LOCKED / 1s WINDOW" : "CHANNEL FIELD / DWELL");
    d.drawFastHLine(5,kBodyTop+17,230,kDimGreen);
    const size_t start = (cursor / 4)*4;
    for (size_t row = 0; row < 4 && start+row < readings.size(); ++row) {
        const size_t index = start+row; const auto &r = readings[index];
        const int y = kBodyTop+25+row*17;
        const bool selected = index == cursor;
        const uint16_t bg = selected ? rgb565(0x39202C) : kVoidInk;
        const uint16_t colour = selected ? kSignalPink : kPaperPhosphor;
        if (selected) { d.fillRect(4,y-2,232,15,bg); d.drawRect(4,y-2,232,15,kSignalPink); }
        d.setTextColor(colour,bg); d.setCursor(7,y);
        if (r.valid) d.printf("%s CH %-3u %lu",selected ? ">" : " ",r.ch,(unsigned long)r.pkts);
        else d.printf("%s CH %-3u --",selected ? ">" : " ",r.ch);
        const uint32_t age = now-r.received_ms;
        d.setCursor(165,y);
        d.setTextColor(!s.active() || age >= 3000 ? kMutedSlate : colour,bg);
        if (!r.valid) d.print("WAIT");
        else d.printf("%lus",(unsigned long)(age/1000));
        d.fillRect(201,y+2,28,4,kTrackDark);
        if (r.valid && r.pkts) d.fillRect(201,y+2,static_cast<uint64_t>(r.pkts)*28/scale,4,selected ? kSignalPink : kFieldGreen);
    }
    if (readings.empty()) {
        d.setCursor(8,kBodyTop+40); d.setTextColor(kMutedSlate,kVoidInk);
        d.print(s.active() ? "WAITING FOR CHANNEL DATA" : "NO CHANNEL DATA / START s");
    }
    d.drawFastHLine(5,kBodyTop+89,230,kDimGreen);
    d.setCursor(6,kBodyTop+93); d.setTextColor(kCalibrationYellow,kVoidInk);
    d.print(s.locked() ? "` RELEASE / EXTERNAL HISTORY" : "ENTER LOCK / ; . FOCUS");
    endChrome(chrome);
}
}
