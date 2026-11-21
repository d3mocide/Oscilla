/* Paired advertiser identity and measured RSSI samples, never frame estimates. */
#include "ui/bt_detail_view.h"
#include "ui/canvas.h"
#include "ui/display_text.h"
#include "ui/external_instrument.h"
#include "ui/observatory_chrome.h"
namespace ui {
void drawBtDetailView(const model::BtInspection &p,const model::BtModel &bt,uint32_t now,const ChromeState &chrome)
{
    beginChrome(chrome);auto &d=canvas();const auto &r=p.row();
    if(!p.active()){d.setTextSize(1);d.setCursor(4,kBodyTop+20);d.print("NO ADVERTISER SELECTED");endChrome(chrome);return;}
    instrument::Canvas c(d,0);
    d.fillRect(0,kBodyTop,d.width(),20,kPanelDark);
    d.fillRect(0,kBodyTop,2,20,r.tracker?kSignalPink:kFieldGreen);
    d.drawFastHLine(0,kBodyTop+20,d.width(),kLineBorder);
    observatory::obsText(c,6,kBodyTop+4,r.name.empty()?"UNNAMED DEVICE":r.name,observatory::FontId::Title,kPaperPhosphor,230);
    d.setTextSize(1);d.setCursor(4,kBodyTop+24);d.setTextColor(kCalibrationYellow,kVoidInk);d.print(printable(r.mac,17).c_str());
    d.setCursor(4,kBodyTop+37);d.setTextColor(kPaperPhosphor,kVoidInk);
    if(!p.vendor().empty())d.printf("MFR %s (%s)",printable(r.mfr,6).c_str(),printable(p.vendor(),22).c_str());
    else d.printf("COMPANY ID %s",printable(r.mfr.empty()?"UNKNOWN":r.mfr,12).c_str());
    d.setCursor(4,kBodyTop+51);d.printf("%ddBm / LAST OBS %lus",r.rssi,(unsigned long)((now-r.observed_ms)/1000));
    d.setCursor(4,kBodyTop+65);d.printf("%s / %u SAMPLES",r.tracker?"TRACKER FLAG":"ADVERTISER",(unsigned)p.history().size());
    d.setCursor(4,kBodyTop+79);d.setTextColor(kMutedSlate,kVoidInk);d.print(p.stateLabel(bt,now));
    d.setCursor(4,kBodyTop+91);d.setTextColor(kCalibrationYellow,kVoidInk);d.print("r RESCAN / s PAUSE / ` LIST");
    endChrome(chrome);
}
void drawExternalBtDetailStrip(lgfx::LGFX_Sprite &d, int top, const model::BtInspection &p,
                               const model::BtModel &bt, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const auto &r = p.row();
    const bool receiving = bt.scanning() || bt.continuousActive() || bt.airtagActive();
    const char *st = p.stateLabel(bt, now);
    const BadgeTone tone = !p.active() ? BadgeTone::Cached
                         : receiving ? BadgeTone::Live : BadgeTone::Cached;

    obsFrame(c, ModeSigil::WaveScope, "ADVERTISER // RSSI HISTORY", st, tone);
    obsDock(c, receiving ? kFieldGreen : kMutedSlate, true);

    if (!p.active()) {
        obsMicro(c, 160, 84, "NO ADVERTISER SELECTED", kCalibrationYellow, kVoidInk, Align::Center);
        obsText(c, 12, 164, "ADVERTISER DOSSIER", FontId::Title, kPaperPhosphor, 296);
        obsBay(c, 12, 94, "RSSI / dBm", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "AGE / s", "--", kMutedSlate);
        obsBay(c, 224, 84, "SOURCES", "--", kPaperPhosphor);
        return;
    }

    constexpr int x0 = 36, y0 = 46, w = 136, h = 80;
    for (int i = 0; i <= 4; ++i) {
        const int yy = y0 + (h * i) / 4;
        if (i == 4) c.line(x0, yy, x0 + w, yy, kDimGreen);
        else obsDashedH(c, x0, x0 + w, yy, kTrackDark, 2, 3);
    }
    for (int i = 0; i <= 4; ++i) {
        obsDashedV(c, x0 + (w * i) / 4, y0, y0 + h, kTrackDark, 2, 3);
    }
    obsMicro(c, 32, y0 - 3, "-20", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 32, y0 + h / 2 - 3, "-60", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 32, y0 + h - 4, "-100", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, x0, 32, "120S REAL SAMPLES", kDimGreen, kVoidInk);

    const auto plot = model::signalPlot(p.history(), now, receiving);
    int lastx = 0, lasty = 0;
    const uint16_t trace_col = receiving ? kFieldGreen : kMutedSlate;
    for (size_t i = 0; i < plot.size; ++i) {
        const auto &point = plot.points[i];
        const int x = x0 + int(uint64_t(120000 - point.age) * w / 120000);
        const int y = y0 + h - (std::max(-100, std::min(-20, point.rssi)) + 100);
        c.line(x, y + 2, x, y0 + h - 1, kTrackDark);
        if (point.connected) {
            c.line(lastx, lasty + 1, x, y + 1, kDimGreen);
            c.line(lastx, lasty, x, y, trace_col);
        }
        if (i + 1 == plot.size) {
            c.ring(x, y, 5, kSignalPink);
            c.rect(x - 2, y - 2, 5, 5, kCalibrationYellow);
        } else {
            c.rect(x - 1, y - 1, 3, 3, trace_col);
        }
        lastx = x; lasty = y;
    }
    if (plot.size == 0) {
        obsMicro(c, x0 + w / 2, y0 + h / 2 - 3, "NO RECENT SAMPLES", kCalibrationYellow, kVoidInk, Align::Center);
    }
    obsMicro(c, x0, 134, "-120s", kMutedSlate, kVoidInk);
    obsMicro(c, x0 + w, 134, receiving ? "NOW" : "LAST",
             receiving ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Right);

    /* Right-side advertiser metadata card. */
    obsCard(c, 182, 36, 122, 103, r.tracker ? kSignalPink : kCalibrationYellow);
    obsMicro(c, 190, 42, "COMPANY ID", kMutedSlate, kPanelDark);
    obsText(c, 190, 55, r.mfr.empty() ? "UNKNOWN" : obsUpper(safe(r.mfr, 10)),
            FontId::Title, kPaperPhosphor, 106);
    c.line(190, 76, 296, 76, kLineBorder);
    obsBadge(c, 190, 81, 86, 14, r.tracker ? "TRACKER FLAG" : "ADVERTISER",
             r.tracker ? kPanelDark : kTrackDark,
             r.tracker ? kSignalPink : kDimGreen,
             r.tracker ? kSignalPink : kFieldGreen);
    obsMicro(c, 190, 102, std::to_string(p.history().size()) + " REAL SAMPLES", kFieldGreen, kPanelDark);
    obsMicro(c, 190, 120, r.first_sighting ? "FIRST SIGHTING" : "SCAN SNAPSHOT", kMutedSlate, kPanelDark);

    /* Lower Reading Dock. */
    obsText(c, 12, 164, r.name.empty() ? "UNNAMED DEVICE" : r.name, FontId::Title, kPaperPhosphor, 296);
    obsText(c, 12, 184, r.mac, FontId::Mono, kMutedSlate, 180);
    obsMicro(c, 308, 186,
             !p.vendor().empty() ? obsUpper(safe(p.vendor(), 20)) : "RAW ID / NO VENDOR",
             !p.vendor().empty() ? kFieldGreen : kMutedSlate,
             kPanelDark, Align::Right);

    obsBay(c, 12, 94, "RSSI / dBm", std::to_string(r.rssi), kCalibrationYellow);
    obsBay(c, 122, 86, "AGE / s", std::to_string((now - r.observed_ms) / 1000), kMutedSlate, "s");
    obsBay(c, 224, 84, "SOURCES", std::to_string(r.n), kPaperPhosphor);
}
}
