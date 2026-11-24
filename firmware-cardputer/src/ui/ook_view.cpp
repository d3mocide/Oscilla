/* ook_view.cpp — see ook_view.h. Layout follows subghz_view.cpp. */
#include "ui/ook_view.h"

#include <M5Cardputer.h>

#include <cstdio>
#include <cstdlib>

#include "ui/canvas.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {
namespace {

/* "12s", "4m", "2h": fits in four columns. */
void formatAge(uint32_t now_ms, uint32_t then_ms, char *out, size_t cap)
{
    const uint32_t s = now_ms >= then_ms ? (now_ms - then_ms) / 1000u : 0u;
    if (s < 100) std::snprintf(out, cap, "%lus", (unsigned long)s);
    else if (s < 6000) std::snprintf(out, cap, "%lum", (unsigned long)(s / 60));
    else std::snprintf(out, cap, "%luh", (unsigned long)(s / 3600));
}

/* "-1.5C" / "21.3C": tenths without floating point. */
void formatTemp(int c10, char *out, size_t cap)
{
    const int a = std::abs(c10);
    std::snprintf(out, cap, "%s%d.%dC", c10 < 0 ? "-" : "", a / 10, a % 10);
}

void drawRow(M5Canvas &d, const model::OokDevice &dev, int y, bool sel, uint32_t now_ms)
{
    const auto &p = dev.last;
    const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
    drawRowHighlight(d, y, sel);
    d.setTextColor(kMutedSlate, bg);
    d.setCursor(6, y);
    d.print(model::ookDecoderShort(p.decoder));
    d.setTextColor(sel ? kCalibrationYellow : kPaperPhosphor, bg);
    d.setCursor(30, y);
    if (p.decoder == model::OokDecoder::LaCrosse) {
        char t[24];
        formatTemp(p.temp_c10, t, sizeof t);
        d.printf("%u/%u %s %u%%", p.id, p.ch, t, p.hum);
        if (p.batt_low) {
            d.setTextColor(kSignalPink, bg);
            d.print(" LOW");
        }
    } else {
        char code[11] = {};
        for (size_t i = 0; i < p.hex.size() && i + 1 < sizeof code; ++i) {
            const char c = p.hex[i];
            code[i] = c >= 'a' && c <= 'f' ? static_cast<char>(c - 'a' + 'A') : c;
        }
        d.print(code);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" x%lu", (unsigned long)dev.count);
    }
    d.setCursor(160, y);
    if (p.has_rssi) {
        d.setTextColor(rssiColour(p.rssi), bg);
        d.printf("%d", p.rssi);
    } else {
        d.setTextColor(kMutedSlate, bg);
        d.print("--");
    }
    char age[8];
    formatAge(now_ms, p.received_ms, age, sizeof age);
    d.setTextColor(kMutedSlate, bg);
    d.setCursor(204, y);
    d.print(age);
}

void drawDetail(M5Canvas &d, const model::OokDevice &dev, uint32_t now_ms)
{
    const auto &p = dev.last;
    char first[8];
    formatAge(now_ms, dev.first_ms, first, sizeof first);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(5, kDetailRowY);
    if (p.decoder == model::OokDecoder::LaCrosse) {
        d.printf("TX141THBv2 x%lu %s%s FIRST %s", (unsigned long)dev.count, p.batt_low ? "BATT LOW" : "BATT OK",
                 p.test ? " TEST" : "", first);
    } else {
        d.printf("PT2262/EV1527 TE %uus x%lu FIRST %s", p.te_us, (unsigned long)dev.count, first);
    }
}

}  // namespace

void drawOokView(const model::OokModel &m, size_t cursor, uint32_t now_ms, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(5, kBodyTop + 3);
    d.setTextColor(kPaperPhosphor, kVoidInk);
    if (m.freqKhz()) d.printf("%lu.%03lu MHz", (unsigned long)(m.freqKhz() / 1000), (unsigned long)(m.freqKhz() % 1000));
    else d.print("433 MHz OOK");
    d.setCursor(88, kBodyTop + 3);
    d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
    d.print(m.active() ? "LIVE" : "IDLE");
    d.setCursor(130, kBodyTop + 3);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("DEV %u UNK %lu", static_cast<unsigned>(m.devices().size()), (unsigned long)m.unknownTrains());
    d.drawFastHLine(5, kBodyTop + 14, 230, kDimGreen);

    const auto &devs = m.devices();
    const size_t start = listFirstVisible(cursor, kListVisibleRows);
    for (size_t row = 0; row < static_cast<size_t>(kListVisibleRows) && start + row < devs.size(); ++row) {
        const size_t i = start + row;
        drawRow(d, devs[i], kBodyTop + 20 + static_cast<int>(row) * kListRowHeight, i == cursor, now_ms);
    }
    if (devs.empty()) {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.setCursor(8, kBodyTop + 40);
        d.print(m.active() ? "LISTENING FOR 433 MHz REMOTES/SENSORS" : "s START 433 MHz OOK RX");
    } else if (cursor < devs.size()) {
        drawDetail(d, devs[cursor], now_ms);
    }

    d.drawFastHLine(5, kBodyTop + 86, 230, kDimGreen);
    d.setCursor(5, kHintRowY);
    const auto &h = m.health();
    if (h.valid && h.hw_fault) {
        d.setTextColor(kFaultRed, kVoidInk);
        d.print("RADIO FAULT / CHECK LINK STATUS");
    } else if (h.valid && (h.overflows || h.dropped || h.errors || h.ocp_drop)) {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.printf("LOSS OVF %lu DROP %lu ERR %lu", (unsigned long)h.overflows,
                 (unsigned long)(h.dropped + h.ocp_drop), (unsigned long)h.errors);
    } else {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(m.active() ? "s STOP / ; . SELECT" : "s START / ; . SELECT");
    }
    endChrome(chrome);
}

}  // namespace ui
