#include "ui/external_bt_view.h"
#include "ui/theme.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace ui {
namespace {
std::string safe(const std::string &s,size_t limit)
{
    std::string out;
    for(unsigned char c:s) {if(out.size()==limit) break;out+=(c>=32 && c<=126) ? char(c) : '?';}
    return out;
}
}
void drawExternalBtStrip(lgfx::LGFX_Sprite &d,int top,const model::BtModel &bt,size_t cursor)
{
    constexpr uint16_t panel=rgb565(0x080C08),border=rgb565(0x1D2A1F);
    auto text=[&](int x,int y,uint16_t colour,const char *s) {
        d.setTextSize(1);d.setTextColor(colour,panel);d.setCursor(x,y-top);d.print(s);
    };
    auto box=[&](int x,int y,int w,int h) {d.fillRect(x,y-top,w,h,panel);d.drawRect(x,y-top,w,h,border);};
    const auto &rows=bt.devices();
    const bool active=bt.scanning() || bt.continuousActive() || bt.airtagActive();
    int strongest=-128;unsigned trackers=0;
    for(const auto &r:rows) {strongest=std::max(strongest,r.rssi);trackers+=r.tracker;}
    char value[64];d.fillScreen(kVoidInk);box(4,3,312,30);
    text(8,6,kMutedSlate,"BLE FIELD");snprintf(value,sizeof value,"%u %s",(unsigned)rows.size(),active ? "OBS" : "CACHED");text(8,20,kCalibrationYellow,value);
    text(116,6,kMutedSlate,"STRONGEST");
    if(rows.empty()) snprintf(value,sizeof value,"-- dBm");else snprintf(value,sizeof value,"%d dBm",strongest);
    text(116,20,kPaperPhosphor,value);
    text(226,6,kMutedSlate,"TRACKER ROWS");snprintf(value,sizeof value,"%u",trackers);text(226,20,kSignalPink,value);
    box(4,36,174,137);box(181,36,135,137);
    text(8,40,kMutedSlate,"RSSI / INDEX FIELD");text(185,40,kMutedSlate,"ADVERTISERS");
    for(int radius:{18,32,46,56}) d.drawCircle(91,106-top,radius,border);
    d.drawLine(35,106-top,147,106-top,border);d.drawLine(91,50-top,91,162-top,border);
    text(95,85,kMutedSlate,"-40");text(95,68,kMutedSlate,"-70");text(95,54,kMutedSlate,"-100");
    d.fillCircle(91,106-top,2,kCalibrationYellow);
    for(size_t i=0;i<rows.size();++i) {
        const double angle=6.283185307179586*i/std::max(size_t(1),rows.size());
        const int radius=std::max(7,std::min(56,7+(-rows[i].rssi-30)*49/70));
        const int x=91+int(std::cos(angle)*radius),y=106+int(std::sin(angle)*radius);
        const uint16_t colour=i==cursor ? kCalibrationYellow : rows[i].tracker ? kSignalPink : kFieldGreen;
        d.fillCircle(x,y-top,i==cursor ? 3 : 1,colour);
        if(i==cursor) d.drawCircle(x,y-top,5,colour);
    }
    if(rows.empty()) text(40,115,kMutedSlate,"NO ADVERTISERS");
    const size_t first=cursor>=6 ? cursor-5 : 0;
    for(size_t i=first;i<rows.size() && i<first+6;++i) {
        const int y=54+(i-first)*18;const auto &r=rows[i];
        if(i==cursor) d.fillRect(184,y-2-top,129,17,kSelectionGlow);
        const auto label=safe(r.name.empty() ? r.mac : r.name,13);
        text(185,y,i==cursor ? kCalibrationYellow : kPaperPhosphor,label.c_str());
        snprintf(value,sizeof value,"%d / %s",r.rssi,r.tracker ? "TRACKER" : "ADV");text(185,y+8,kMutedSlate,value);
    }
    box(4,176,312,37);
    if(cursor<rows.size()) {
        const auto &r=rows[cursor];
        text(8,181,kCalibrationYellow,safe(r.name.empty() ? r.mac : r.name,20).c_str());
        text(8,199,kMutedSlate,safe(r.mac,17).c_str());
        snprintf(value,sizeof value,"%d dBm",r.rssi);text(139,181,kPaperPhosphor,value);
        snprintf(value,sizeof value,"%lu SEEN",(unsigned long)r.n);text(139,199,kMutedSlate,value);
        text(226,181,kMutedSlate,"MFR ID");text(226,199,kFieldGreen,r.mfr.empty() ? "UNKNOWN" : safe(r.mfr,4).c_str());
    } else text(8,187,kMutedSlate,"NO SELECTED ADVERTISER");
    text(8,219,kMutedSlate,active ? "PASSIVE / OBSERVING" : "CACHED / SCAN STOPPED");
    text(8,230,kMutedSlate,"INDEX ANGLE / NO BEARING");
}
}
