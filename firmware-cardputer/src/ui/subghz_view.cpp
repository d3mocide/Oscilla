/* Rendered reference: master guide Observe / LoRa RX deck. */
#include "ui/subghz_view.h"
#include <M5Cardputer.h>
#include <algorithm>
#include <cstring>
#include "model/lora_profiles.h"
#include "ui/canvas.h"
#include "ui/theme.h"
#include "ui/list_row.h"

namespace ui {
namespace {
std::string profileLabel(const std::string &token) {
    for (const auto &p:model::kLoraProfiles) if(token==p.token) return p.label;
    return token.empty() || token == "manual" ? "MANUAL" : token;
}

void drawSweepMode(const model::LoraSweepModel &sw, size_t preset_idx, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);
    const auto &preset = model::kLoraSweepPresets[preset_idx % model::kLoraSweepPresetCount];

    d.setCursor(5, kBodyTop + 3);
    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.print(preset.label);
    d.setCursor(116, kBodyTop + 3);
    if (sw.hasData()) {
        const uint32_t pk_khz = sw.binFreqKhz(sw.peakBin());
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.printf("PK %lu.%02lu %ddB",
                 (unsigned long)(pk_khz / 1000),
                 (unsigned long)((pk_khz % 1000) / 10),
                 static_cast<int>(sw.maxPeakDbm()));
    } else {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(sw.active() ? "SWEEPING..." : "IDLE");
    }
    d.drawFastHLine(5, kBodyTop + 14, 230, kDimGreen);

    constexpr int gx = 8, gy = kBodyTop + 18, gw = 224, gh = 62;
    d.drawFastHLine(gx, gy + gh, gw, kDimGreen);
    if (sw.hasData() && sw.bins() > 0) {
        const size_t n = sw.bins();
        for (size_t i = 0; i < n; ++i) {
            const int x0 = gx + static_cast<int>(i * gw / n);
            const int x1 = gx + static_cast<int>((i + 1) * gw / n);
            const int bw = std::max(1, x1 - x0 - 1);
            const int cur = std::max(-120, std::min(-40, static_cast<int>(sw.currentDbm(i))));
            const int pk = std::max(-120, std::min(-40, static_cast<int>(sw.peakDbm(i))));
            const int ch = (cur + 120) * (gh - 2) / 80;
            const int py = gy + gh - 1 - ((pk + 120) * (gh - 2) / 80);
            if (ch > 0) {
                d.fillRect(x0, gy + gh - ch, bw, ch, sw.active() ? kFieldGreen : kMutedSlate);
            }
            d.fillRect(x0, py, bw, 2, i == sw.peakBin() ? kSignalPink : kCalibrationYellow);
        }
    } else {
        d.setCursor(12, kBodyTop + 42);
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(sw.active() ? "READING SX1262 RSSI..." : "s START SWEEP / x BAND");
    }

    d.drawFastHLine(5, kBodyTop + 86, 230, kDimGreen);
    d.setCursor(5, kHintRowY);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print(sw.active() ? "s STOP / r PEAK / w PKTS" : "s SWEEP / x BAND / w PKTS");
    endChrome(chrome);
}
}
void drawSubGhzView(const model::LoraModel &m, size_t cursor, const ChromeState &chrome,
                    const char *pending, const model::LoraSweepModel *sweep,
                    bool sweep_mode, size_t sweep_preset_idx)
{
    if (sweep_mode && sweep) {
        drawSweepMode(*sweep, sweep_preset_idx, chrome);
        return;
    }
    auto &d=canvas(); beginChrome(chrome); d.setTextSize(1);
    d.setTextColor(kPaperPhosphor,kVoidInk); d.setCursor(5,kBodyTop+3);
    if(m.hasConfig()) d.printf("%lu.%03lu MHz",(unsigned long)(m.freqHz()/1000000),(unsigned long)(m.freqHz()%1000000/1000));
    else d.print("CONFIG REQUIRED");
    d.setCursor(149,kBodyTop+3); d.setTextColor(kFieldGreen,kVoidInk);
    if(m.hasConfig()) d.printf("BW%d SF%d",m.bwKhz(),m.sf());
    d.drawFastHLine(5,kBodyTop+14,230,kDimGreen);
    const auto &packets=m.packets(); const size_t start=listFirstVisible(cursor,kListVisibleRows);
    for(size_t row=0;row<static_cast<size_t>(kListVisibleRows) && start+row<packets.size();++row) {
        const size_t index=start+row; const auto &p=packets[index];
        const int y=kBodyTop+20+static_cast<int>(row)*kListRowHeight; const bool sel=index==cursor;
        const uint16_t bg=sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);
        d.setTextColor(sel ? kCalibrationYellow : kPaperPhosphor,bg); d.setCursor(6, y);
        if(p.framing==model::Framing::Unknown) d.printf("RAW %uB",p.len);
        else d.printf("%.10s?",model::framingName(p.framing));
        d.setCursor(116,y); d.printf("%d dBm",p.rssi);
        d.setCursor(184,y); d.setTextColor(p.snr>=0 ? kFieldGreen : kSignalPink,bg);
        d.printf("%+.1f",(double)p.snr);
    }
    if(packets.empty()) {
        d.setTextColor(kMutedSlate,kVoidInk); d.setCursor(8,kBodyTop+40);
        d.print(m.active() ? "LISTENING FOR LORA FRAMES" : m.hasConfig() ? "s START / x PROFILE / c APPLY" : "x PROFILE / c CONFIGURE");
    }
    d.drawFastHLine(5,kBodyTop+86,230,kDimGreen);
    d.setCursor(5,kHintRowY); d.setTextColor(kMutedSlate,kVoidInk);
    if(!m.hasConfig() || std::strcmp(pending,profileLabel(m.profile()).c_str())!=0) {
        d.setTextColor(kCalibrationYellow,kVoidInk); d.printf("c APPLY %.20s / x NEXT",pending);
    } else if(m.health().valid && m.health().hw_fault) {
        d.setTextColor(kFaultRed,kVoidInk); d.print("RADIO FAULT / CHECK LINK STATUS");
    } else d.print("w SWEEP / m MARK / x PROF / c SET");
    endChrome(chrome);
}
}
