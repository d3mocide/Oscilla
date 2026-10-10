#include "ui/external_system_view.h"
#include "ui/theme.h"
#include <cstdio>
#include <cmath>
namespace ui {
namespace {
void mark(lgfx::LGFX_Sprite &d,int cx,int cy,int top,int scale)
{
    auto line=[&](int x,int y,int xx,int yy,uint16_t c) {d.drawLine(cx+x*scale,cy+y*scale-top,cx+xx*scale,cy+yy*scale-top,c);};
    const int hex[][2]={{0,-42},{36,-21},{36,21},{0,42},{-36,21},{-36,-21}};
    for(int i=0;i<6;++i) line(hex[i][0],hex[i][1],hex[(i+1)%6][0],hex[(i+1)%6][1],kFieldGreen);
    for(int axis=0;axis<2;++axis) {
        int lastx=0,lasty=0;
        for(int i=0;i<=64;++i) {
            const double a=i*6.283185307179586/64;
            const int x=std::cos(a)*(axis ? 14 : 34),y=std::sin(a)*(axis ? 34 : 14);
            if(i) line(lastx,lasty,x,y,kFieldGreen);
            lastx=x;lasty=y;
        }
    }
    const int lens[][2]={{-16,0},{-10,-7},{0,-9},{10,-7},{16,0},{10,7},{0,9},{-10,7}};
    for(int i=0;i<8;++i) line(lens[i][0],lens[i][1],lens[(i+1)%8][0],lens[(i+1)%8][1],kPaperPhosphor);
    d.fillCircle(cx,cy-top,5*scale,kCalibrationYellow);
}
std::string safe(const std::string &s,size_t cap)
{
    std::string out;for(unsigned char c:s) {if(out.size()==cap) break;out+=(c>=32 && c<=126) ? char(c) : '?';}return out;
}
}
void drawExternalStartupStrip(lgfx::LGFX_Sprite &d,int top)
{
    d.fillScreen(kVoidInk);mark(d,160,93,top,1);
    d.setTextSize(2);d.setTextColor(kPaperPhosphor,kVoidInk);d.setCursor(118,153-top);d.print("OSCILLA");
    d.setTextSize(1);d.setTextColor(kFieldGreen,kVoidInk);d.setCursor(106,181-top);d.print("OBSERVE THE NOISE.");
    d.setTextColor(kMutedSlate,kVoidInk);d.setCursor(100,204-top);d.print("VIEWPORT STARTING");
}
void drawExternalSystemStrip(lgfx::LGFX_Sprite &d,int top,const model::SystemStatus &s,uint32_t now)
{
    auto text=[&](int x,int y,uint16_t c,const char *v) {d.setTextSize(1);d.setTextColor(c,kPanelDark);d.setCursor(x,y-top);d.print(v);};
    auto box=[&](int x,int y,int w,int h) {d.fillRect(x,y-top,w,h,kPanelDark);d.drawRect(x,y-top,w,h,kLineBorder);};
    const char *state=s.link==ocp::LinkState::Ready ? "READY" : s.link==ocp::LinkState::HelloSent ? "SYNCING" : s.link==ocp::LinkState::Incompatible ? "MISMATCH" : "OFFLINE";
    const uint16_t colour=s.link==ocp::LinkState::Ready ? kFieldGreen : kCalibrationYellow;
    char v[80];d.fillScreen(kVoidInk);box(4,3,312,30);
    text(8,7,kMutedSlate,s.info ? "SYSTEM HEALTH" : "GROVE LINK");text(8,20,colour,state);
    snprintf(v,sizeof v,"%lus",(unsigned long)(now/1000));text(125,7,kMutedSlate,"DECK UPTIME");text(125,20,kPaperPhosphor,v);
    text(234,7,kMutedSlate,"RX ONLY");text(234,20,kFieldGreen,"PASSIVE");
    box(4,36,112,137);mark(d,60,95,top,1);text(12,150,kMutedSlate,"TWO MACHINES");text(12,162,kFieldGreen,"ONE CONTRACT");
    box(119,36,197,137);
    if(s.info) {
        text(126,43,kMutedSlate,"DECK FREE / MINIMUM");snprintf(v,sizeof v,"%lu / %lu KB",(unsigned long)(s.deck_heap/1024),(unsigned long)(s.deck_min/1024));text(126,59,kPaperPhosphor,v);
        text(126,80,kMutedSlate,"PROBE HEAP SNAPSHOT");
        if(s.probe_valid) snprintf(v,sizeof v,"%lu / %lu KB",(unsigned long)(s.probe_heap/1024),(unsigned long)(s.probe_total/1024));else snprintf(v,sizeof v,"WAITING FOR STATUS");text(126,96,s.probe_valid ? kFieldGreen : kCalibrationYellow,v);
        text(126,119,kMutedSlate,"PROBE UPTIME SNAPSHOT");
        if(s.probe_valid) snprintf(v,sizeof v,"%llus",(unsigned long long)(s.probe_uptime/1000));else snprintf(v,sizeof v,"--");text(126,135,kPaperPhosphor,v);
    } else {
        text(126,43,kMutedSlate,"PROBE FIRMWARE");text(126,59,kPaperPhosphor,safe(s.probe.fw+" "+s.probe.ver,28).c_str());
        text(126,81,kMutedSlate,"PROTOCOL / HANDSHAKES");snprintf(v,sizeof v,"%d / %lu",s.probe.proto,(unsigned long)s.stats.hellos);text(126,97,kFieldGreen,v);
        text(126,119,kMutedSlate,"ADVERTISED CAPABILITIES");text(126,135,kPaperPhosphor,safe(s.probe.caps,28).c_str());
        if(s.probe.caps.size()>28) text(126,151,kMutedSlate,safe(s.probe.caps.substr(28),28).c_str());
    }
    box(4,176,312,37);text(8,181,kMutedSlate,"EVENTS / RESETS");snprintf(v,sizeof v,"%lu / %lu",(unsigned long)s.stats.events,(unsigned long)s.stats.resets);text(8,199,kPaperPhosphor,v);
    text(118,181,kMutedSlate,"ERRORS / TIMEOUT");snprintf(v,sizeof v,"%lu / %lu",(unsigned long)s.stats.errors,(unsigned long)s.stats.timeouts);text(118,199,s.stats.errors || s.stats.timeouts ? kFaultRed : kFieldGreen,v);
    text(232,181,kMutedSlate,"NOISE / STRAY");snprintf(v,sizeof v,"%lu / %lu",(unsigned long)s.stats.noise,(unsigned long)s.stats.stray);text(232,199,kMutedSlate,v);
    text(8,219,kMutedSlate,s.info ? "MEASURED SYSTEM DIAGNOSTICS" : "PROBE IDENTITY / LINK COUNTERS");text(8,230,kMutedSlate,"CONTROL REMAINS ON DECK");
}
}
