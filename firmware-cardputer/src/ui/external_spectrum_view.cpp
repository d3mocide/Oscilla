/* Rendered reference: master brand guide, external Packet Monitor Field. */
#include "ui/external_spectrum_view.h"
#include "ui/theme.h"
#include <algorithm>
#include <cstdio>

namespace ui {
namespace {
constexpr uint16_t ground = rgb565(0x060806), panel = rgb565(0x080C08);
constexpr uint16_t plot = rgb565(0x070A07), border = rgb565(0x1D2A1F);
constexpr uint16_t axis = rgb565(0x4B6349);
}
void drawExternalSpectrumStrip(lgfx::LGFX_Sprite &d, int top,
                              const model::SpectrumModel &s, size_t cursor, uint32_t now)
{
    const auto &readings = s.readings();
    if (s.locked()) cursor = 0;
    const auto *focus = cursor < readings.size() ? &readings[cursor] : nullptr;
    const size_t count = std::min<size_t>(readings.size(), 40);
    uint32_t scale = 1, peak = 0;
    bool have_peak = false;
    for (const auto &r : readings) {
        scale = std::max(scale, r.pkts);
        for (size_t i = 0; i < r.history_size; ++i) {
            if (now - r.history[i].received_ms < 30000) {
                scale = std::max(scale, r.history[i].pkts);
                if (&r == focus) { peak = std::max(peak, r.history[i].pkts); have_peak = true; }
            }
        }
    }
    auto rect = [&](int x, int y, int w, int h, uint16_t c) { d.fillRect(x,y-top,w,h,c); };
    auto line = [&](int x1, int y1, int x2, int y2, uint16_t c) { d.drawLine(x1,y1-top,x2,y2-top,c); };
    auto text = [&](int x, int y, uint16_t colour, const char *value, uint16_t bg = ground) {
        d.setTextSize(1); d.setTextColor(colour,bg); d.setCursor(x,y-top); d.print(value);
    };
    char value[48];
    d.fillScreen(ground);
    rect(4,3,312,25,panel); d.drawRect(4,3-top,312,25,border);
    const char *labels[] = {"CHANNEL", "COUNT/WIN", "PEAK/30s", "FRESHNESS"};
    const uint16_t colours[] = {kCalibrationYellow,kPaperPhosphor,kSignalPink,kCalibrationYellow};
    for (int tile = 0; tile < 4; ++tile) {
        const int x = 8 + tile * 78;
        if (tile) line(x-4,6,x-4,24,border);
        text(x,6,kMutedSlate,labels[tile],panel);
        if (!focus) snprintf(value,sizeof value,"--");
        else if (tile == 0) snprintf(value,sizeof value,"CH %u",focus->ch);
        else if (tile == 1) snprintf(value,sizeof value,focus->valid ? "%lu" : "--",(unsigned long)focus->pkts);
        else if (tile == 2) snprintf(value,sizeof value,have_peak ? "%lu" : "--",(unsigned long)peak);
        else if (!focus->valid) snprintf(value,sizeof value,"WAIT");
        else if (!s.active()) snprintf(value,sizeof value,"CACHED");
        else snprintf(value,sizeof value,"%s %lus",now-focus->received_ms < 3000 ? "LIVE" : "AGED",(unsigned long)((now-focus->received_ms)/1000));
        text(x,17,colours[tile],value,panel);
    }
    rect(4,31,312,183,plot); d.drawRect(4,31-top,312,183,border);
    text(10,35,axis,"PACKET HISTORY / 30s",plot);
    text(190,35,kMutedSlate,s.locked() ? "1s WINDOWS" : "DWELL COUNTS",plot);
    for (int tick = 0; tick <= 3; ++tick) {
        const int y = 49 + tick*24;
        snprintf(value,sizeof value,"-%us",tick*10);
        text(7,y,axis,value,plot); line(30,y+4,310,y+4,border);
    }
    const int step = count ? 276/count : 276;
    for (size_t i = 0; i < count; ++i) {
        const auto &r = readings[i]; const int x = 32+i*step;
        if (i == cursor) { rect(x,47,step-1,79,kPanelDark); d.drawRect(x,47-top,step-1,79,kSignalPink); }
        for (size_t j = 0; j < r.history_size; ++j) {
            const auto &sample = r.history[j]; const uint32_t age = now-sample.received_ms;
            if (age >= 30000) continue;
            const uint32_t level = static_cast<uint64_t>(sample.pkts)*100/scale;
            const uint16_t colour = !sample.pkts ? kTrackDark : level >= 75 ? kSignalPink : level >= 40 ? kCalibrationYellow : kDimGreen;
            rect(x+2,49+(age*72/30000),std::max(1,step-5),2,colour);
        }
    }
    text(9,130,axis,"CHANNEL COUNT ENVELOPE",plot);
    snprintf(value,sizeof value,"MAX %lu",(unsigned long)scale); text(246,130,kMutedSlate,value,plot);
    for (int tick = 0; tick < 3; ++tick) line(30,147+tick*22,310,147+tick*22,border);
    line(30,142,30,197,axis); line(30,197,310,197,axis);
    int last_x = 0, last_y = 0; bool last_valid = false;
    for (size_t i = 0; i < count; ++i) {
        const auto &r = readings[i]; const int x = 32+i*step+step/2;
        const int y = 194-static_cast<uint64_t>(r.pkts)*46/scale;
        const bool fresh = r.valid && now-r.received_ms < 3000;
        if (r.valid && last_valid) line(last_x,last_y,x,y,s.active() && fresh ? kFieldGreen : kMutedSlate);
        if (r.valid) { line(x,194,x,y,kDimGreen); d.fillCircle(x,y-top,2,i==cursor ? kSignalPink : kFieldGreen); }
        if (i==cursor) {
            line(x,197,x,201,kCalibrationYellow);
            snprintf(value,sizeof value,"%u",r.ch);
            text(std::max(32,std::min(286,x-8)),203,kCalibrationYellow,value,plot);
        }
        last_x=x; last_y=y; last_valid=r.valid;
    }
    if (!count) text(46,80,kMutedSlate,"WAITING FOR MEASURED CHANNELS",plot);
    rect(4,217,224,20,panel); d.drawRect(4,217-top,224,20,border);
    if (focus) snprintf(value,sizeof value,"ACTIVE CH %u / %s",focus->ch,focus->band5 ? "5 GHz" : "2.4 GHz");
    else snprintf(value,sizeof value,"NO CHANNEL SELECTED");
    text(8,220,kPaperPhosphor,value,panel);
    text(8,229,kMutedSlate,s.active() ? "PASSIVE / MEASURED COUNTS" : "CACHED / RECEIVER STOPPED",panel);
}
}
