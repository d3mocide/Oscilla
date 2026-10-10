/* Rendered reference: master guide Observe / LoRa RX deck. */
#include "ui/subghz_view.h"
#include <M5Cardputer.h>
#include <cstring>
#include "model/lora_profiles.h"
#include "ui/canvas.h"
#include "ui/theme.h"
#include "ui/list_row.h"

namespace ui {
namespace {
const char *profileLabel(const std::string &token) {
    for (const auto &p:model::kLoraProfiles) if(token==p.token) return p.label;
    return "MANUAL";
}
}
void drawSubGhzView(const model::LoraModel &m, size_t cursor, const ChromeState &chrome,
                    const char *pending)
{
    auto &d=canvas(); beginChrome(chrome); d.setTextSize(1);
    d.setTextColor(kPaperPhosphor,kVoidInk); d.setCursor(5,kBodyTop+5);
    if(m.hasConfig()) d.printf("%lu.%03lu MHz",(unsigned long)(m.freqHz()/1000000),(unsigned long)(m.freqHz()%1000000/1000));
    else d.print("CONFIG REQUIRED");
    d.setCursor(149,kBodyTop+5); d.setTextColor(kFieldGreen,kVoidInk);
    if(m.hasConfig()) d.printf("BW%d SF%d",m.bwKhz(),m.sf());
    d.drawFastHLine(5,kBodyTop+17,230,kDimGreen);
    const auto &packets=m.packets(); const size_t start=(cursor/4)*4;
    for(size_t row=0;row<4 && start+row<packets.size();++row) {
        const size_t index=start+row; const auto &p=packets[index];
        const int y=kBodyTop+25+row*15; const bool sel=index==cursor;
        const uint16_t bg=sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);
        if(sel) { d.fillRect(4,y-2,232,14,bg); d.drawRect(4,y-2,232,14,kCalibrationYellow); }
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
    d.drawFastHLine(5,kBodyTop+84,230,kDimGreen);
    d.setCursor(5,kBodyTop+88); d.setTextColor(kMutedSlate,kVoidInk);
    if(!m.hasConfig() || std::strcmp(pending,profileLabel(m.profile()))!=0) {
        d.setTextColor(kCalibrationYellow,kVoidInk); d.printf("c APPLY %.20s / x NEXT",pending);
    } else if(m.health().valid && m.health().hw_fault) {
        d.setTextColor(kFaultRed,kVoidInk); d.print("RADIO FAULT / CHECK LINK STATUS");
    } else if(m.health().valid) {
        const auto &h=m.health();
        d.setTextColor(h.irq_drop || h.radio_drop || h.ocp_drop ? kCalibrationYellow : kMutedSlate,kVoidInk);
        d.printf("RX%u C%u H%u D%u/%u/%u F%u",(unsigned)h.rx,(unsigned)h.crc_err,
                 (unsigned)h.header_err,(unsigned)h.irq_drop,(unsigned)h.radio_drop,
                 (unsigned)h.ocp_drop,(unsigned)h.hw_fault);
    } else d.printf("RX %lu / %s / GUESS ?",(unsigned long)m.totalCount(),m.active() ? "LISTENING" : "CACHED");
    endChrome(chrome);
}
}
