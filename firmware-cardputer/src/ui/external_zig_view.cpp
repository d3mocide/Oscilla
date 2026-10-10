#include "ui/external_zig_view.h"
#include "ui/theme.h"
#include <algorithm>
#include <cstdio>
namespace ui {
namespace {
std::string safe(const std::string &s,size_t cap) {std::string out;for(unsigned char c:s) {if(out.size()==cap) break;out+=(c>=32 && c<=126) ? char(c) : '?';}return out;}
}
void drawExternalZigStrip(lgfx::LGFX_Sprite &d,int top,const model::ZigModel &m,bool pans,size_t cursor)
{
    auto text=[&](int x,int y,uint16_t c,const char *v) {d.setTextSize(1);d.setTextColor(c,kPanelDark);d.setCursor(x,y-top);d.print(v);};
    auto box=[&](int x,int y,int w,int h) {d.fillRect(x,y-top,w,h,kPanelDark);d.drawRect(x,y-top,w,h,kLineBorder);};
    const size_t n=pans ? m.pans().size() : m.nodes().size();char v[80];d.fillScreen(kVoidInk);box(4,3,312,30);
    text(8,6,kMutedSlate,"802.15.4 MAC");snprintf(v,sizeof v,"%u PANS",(unsigned)m.pans().size());text(8,20,kCalibrationYellow,v);
    text(122,6,kMutedSlate,"NODE INVENTORY");snprintf(v,sizeof v,"%u NODES",(unsigned)m.nodes().size());text(122,20,kPaperPhosphor,v);
    text(238,6,kMutedSlate,"CAPTURE");text(238,20,m.active() ? kFieldGreen : kMutedSlate,m.active() ? "PASSIVE" : "CACHED");
    box(4,36,312,137);text(8,40,kMutedSlate,pans ? "PAN / CHANNELS" : "PAN / SHORT ADDRESS");
    text(163,40,kMutedSlate,"RSSI");text(208,40,kMutedSlate,"LQI");
    const size_t first=cursor>=6 ? cursor-5 : 0;
    for(size_t i=first;i<n && i<first+6;++i) {
        const int y=56+(i-first)*18;
        const int rssi=pans ? m.pans()[i].rssi : m.nodes()[i].rssi;
        const unsigned lqi=pans ? m.pans()[i].lqi : m.nodes()[i].lqi;
        if(i==cursor) d.fillRect(7,y-2-top,306,17,kSelectionGlow);
        const auto label=pans ? m.pans()[i].pan+" / "+m.pans()[i].channels : m.nodes()[i].pan+" / "+m.nodes()[i].short_addr;
        text(8,y,i==cursor ? kCalibrationYellow : kPaperPhosphor,safe(label,23).c_str());
        snprintf(v,sizeof v,"%d",rssi);text(163,y,kPaperPhosphor,v);
        snprintf(v,sizeof v,"%u",lqi);text(208,y,kFieldGreen,v);
        d.fillRect(239,y+2-top,70,4,kLineBorder);d.fillRect(239,y+2-top,lqi*70/255,4,kFieldGreen);
    }
    if(!n) {text(64,94,kCalibrationYellow,"NO MAC INVENTORY YET");text(58,110,kMutedSlate,"NO TOPOLOGY INFERRED");}
    box(4,176,312,37);
    if(cursor<n && pans) {
        const auto &r=m.pans()[cursor];text(8,181,kCalibrationYellow,safe(r.pan,12).c_str());text(8,199,kMutedSlate,safe(r.channels,20).c_str());
        snprintf(v,sizeof v,"%u NODES",r.nodes);text(141,181,kPaperPhosphor,v);text(141,199,kMutedSlate,"REPORTED COUNT");text(240,181,kMutedSlate,"IDENTITY");text(240,199,kFieldGreen,safe(r.proto,10).c_str());
    } else if(cursor<n) {
        const auto &r=m.nodes()[cursor];text(8,181,kCalibrationYellow,safe(r.pan+" / "+r.short_addr,20).c_str());text(8,199,kMutedSlate,safe(r.ext,20).c_str());
        snprintf(v,sizeof v,"%lu SEEN",(unsigned long)r.seen);text(141,181,kPaperPhosphor,v);text(141,199,kMutedSlate,"MAC OBSERVATIONS");text(240,181,kMutedSlate,"ROLE");text(240,199,kFieldGreen,safe(r.role,10).c_str());
    } else text(8,187,kMutedSlate,"WAITING FOR PAN/NODE SNAPSHOT");
    snprintf(v,sizeof v,"%s / BAD ROWS %u",m.active() ? "PASSIVE" : "CACHED",m.malformedRows());text(8,219,kMutedSlate,v);text(8,230,kMutedSlate,"MAC INVENTORY / NO ROUTES");
}
}
