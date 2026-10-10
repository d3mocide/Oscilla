#include "ui/external_contacts_view.h"
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
void drawExternalContactsStrip(lgfx::LGFX_Sprite &d,int top,
    const model::ContactsModel &m,bool probes,size_t cursor,uint32_t now)
{
    constexpr uint16_t panel=rgb565(0x080C08),border=rgb565(0x1D2A1F);
    auto text=[&](int x,int y,uint16_t c,const char *s) {d.setTextSize(1);d.setTextColor(c,panel);d.setCursor(x,y-top);d.print(s);};
    auto box=[&](int x,int y,int w,int h) {d.fillRect(x,y-top,w,h,panel);d.drawRect(x,y-top,w,h,border);};
    const auto &clients=m.clients();const auto &requests=m.probes();
    const size_t count=probes ? requests.size() : clients.size();
    const bool selected=cursor<count;
    const bool have=probes ? m.hasProbeSnapshot() : m.hasClientSnapshot();
    const uint32_t age=now-(probes ? m.probeSnapshotMs() : m.clientSnapshotMs());
    const std::string target=selected && !probes ? clients[cursor].bssid : "";
    char value[80];d.fillScreen(kVoidInk);box(4,3,312,30);
    text(8,6,kMutedSlate,probes ? "PROBE REQUESTS" : "CLIENT LINKS");
    snprintf(value,sizeof value,"%u ROWS",(unsigned)count);text(8,20,kCalibrationYellow,value);
    text(116,6,kMutedSlate,"PACKET TICKER");snprintf(value,sizeof value,"%lu",(unsigned long)m.totalPkts());text(116,20,kPaperPhosphor,value);
    text(226,6,kMutedSlate,"SNAPSHOT");
    if(!have) snprintf(value,sizeof value,"WAITING");
    else if(!m.sniffing()) snprintf(value,sizeof value,"CACHED");
    else snprintf(value,sizeof value,"%lus %s",(unsigned long)(age/1000),age>=10000 ? "AGED" : "SNAP");
    text(226,20,have && age<10000 && m.sniffing() ? kFieldGreen : kCalibrationYellow,value);
    box(4,36,174,137);box(181,36,135,137);
    text(8,40,kMutedSlate,probes ? "REQUEST RSSI FIELD" : "SELECTED BSSID LINKS");text(185,40,kMutedSlate,probes ? "REQUESTERS" : "CLIENTS");
    for(int radius:{14,35,56}) d.drawCircle(91,106-top,radius,border);
    d.drawLine(35,106-top,147,106-top,border);d.drawLine(91,50-top,91,162-top,border);
    text(95,90,kMutedSlate,"-40");text(95,69,kMutedSlate,"-70");text(95,50,kMutedSlate,"-100");
    size_t matching=0;
    for(size_t i=0;i<count;++i) if(probes || clients[i].bssid==target) ++matching;
    size_t index=0;
    for(size_t i=0;i<count;++i) {
        if(!probes && clients[i].bssid!=target) continue;
        const int rssi=probes ? requests[i].rssi : clients[i].rssi;
        const double angle=6.283185307179586*index++/std::max(size_t(1),matching);
        const int radius=std::max(7,std::min(56,7+(-rssi-30)*49/70));
        const int x=91+int(std::cos(angle)*radius),y=106+int(std::sin(angle)*radius);
        const uint16_t c=i==cursor ? kCalibrationYellow : kFieldGreen;
        if(!probes) d.drawLine(91,106-top,x,y-top,kDimGreen);
        d.fillCircle(x,y-top,i==cursor ? 3 : 1,c);
        if(i==cursor) d.drawCircle(x,y-top,5,c);
    }
    d.fillCircle(91,106-top,2,kCalibrationYellow);
    if(!matching) text(30,116,kMutedSlate,"NO SNAPSHOT LINKS");
    text(8,163,kMutedSlate,probes ? "REQUESTS / NO AP LINK" : "BSSID AT CENTRE");
    const size_t first=cursor>=6 ? cursor-5 : 0;
    for(size_t i=first;i<count && i<first+6;++i) {
        const int y=54+(i-first)*18;
        if(i==cursor) d.fillRect(184,y-2-top,129,17,kSelectionGlow);
        const auto &mac=probes ? requests[i].mac : clients[i].mac;
        text(185,y,i==cursor ? kCalibrationYellow : kPaperPhosphor,safe(mac,17).c_str());
        if(probes) snprintf(value,sizeof value,"%d / %s",requests[i].rssi,safe(requests[i].ssid.empty() ? "<wildcard>" : requests[i].ssid,13).c_str());
        else snprintf(value,sizeof value,"%d / CH %u",clients[i].rssi,clients[i].ch);
        text(185,y+8,kMutedSlate,value);
    }
    box(4,176,312,37);
    if(selected) {
        const auto &mac=probes ? requests[cursor].mac : clients[cursor].mac;
        text(8,181,kCalibrationYellow,safe(mac,17).c_str());
        if(probes) text(8,199,kMutedSlate,safe(requests[cursor].ssid.empty() ? "<wildcard>" : requests[cursor].ssid,20).c_str());
        else text(8,199,kMutedSlate,safe(target,17).c_str());
        snprintf(value,sizeof value,"%d dBm",probes ? requests[cursor].rssi : clients[cursor].rssi);text(139,181,kPaperPhosphor,value);
        snprintf(value,sizeof value,"%lu PKTS",(unsigned long)(probes ? requests[cursor].pkts : clients[cursor].pkts));text(139,199,kMutedSlate,value);
        text(226,181,kMutedSlate,probes ? "SSID REQUEST" : "BSSID LINK");
        if(probes) snprintf(value,sizeof value,"NO AP LINK");else snprintf(value,sizeof value,"CH %u %s",clients[cursor].ch,clients[cursor].band5 ? "5G" : "2.4G");
        text(226,199,kFieldGreen,value);
    } else text(8,187,kMutedSlate,"NO SELECTED SNAPSHOT ROW");
    char channel[8];
    if(m.currentChannel()) snprintf(channel,sizeof channel,"%u",m.currentChannel());else snprintf(channel,sizeof channel,"--");
    snprintf(value,sizeof value,"%s / CH %s / BAD %u",m.sniffing() ? "PASSIVE" : "CACHED",channel,m.malformedRows());text(8,219,kMutedSlate,value);
    text(8,230,kMutedSlate,"INDEX ANGLE / NO BEARING");
}
}
