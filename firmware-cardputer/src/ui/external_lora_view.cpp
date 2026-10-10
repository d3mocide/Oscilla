#include "ui/external_lora_view.h"
#include "ui/theme.h"
#include "model/lora_payload_view.h"
#include <algorithm>
#include <cstdio>

namespace ui {
namespace {
constexpr uint16_t ground=rgb565(0x060806), panel=rgb565(0x080C08), border=rgb565(0x1D2A1F);
}
void drawExternalLoraStrip(lgfx::LGFX_Sprite &d, int top,
                           const model::LoraModel &m, size_t cursor, uint32_t now)
{
    const auto &packets = m.packets();
    const auto *p = cursor < packets.size() ? &packets[cursor] : nullptr;
    auto box = [&](int x,int y,int w,int h,uint16_t bg) {
        d.fillRect(x,y-top,w,h,bg); d.drawRect(x,y-top,w,h,border);
    };
    auto text = [&](int x,int y,uint16_t colour,const char *value,uint16_t bg=panel) {
        d.setTextSize(1); d.setTextColor(colour,bg); d.setCursor(x,y-top); d.print(value);
    };
    char value[64];
    d.fillScreen(ground); box(4,3,312,30,panel);
    const char *labels[] = {"MODULATION","RX MHz","SIGNAL","RX FRAME"};
    for (int tile=0;tile<4;++tile) {
        const int x=8+78*tile;
        if(tile) d.drawLine(x-4,6-top,x-4,29-top,border);
        text(x,6,kMutedSlate,labels[tile]);
        if(tile==0) {
            if(m.hasConfig()) snprintf(value,sizeof value,"SF%d BW%d",m.sf(),m.bwKhz());
            else snprintf(value,sizeof value,"NO CONFIG");
        } else if(tile==1) {
            if(m.hasConfig()) snprintf(value,sizeof value,"%lu.%03lu",(unsigned long)(m.freqHz()/1000000),(unsigned long)(m.freqHz()%1000000/1000));
            else snprintf(value,sizeof value,"--");
        } else if(tile==2) {
            if(p) snprintf(value,sizeof value,"%d dBm",p->rssi); else snprintf(value,sizeof value,"--");
        } else {
            if(p) snprintf(value,sizeof value,"%u BYTES",p->len); else snprintf(value,sizeof value,"WAITING");
        }
        text(x,17,tile==0||tile==2 ? kCalibrationYellow : kPaperPhosphor,value);
        if(tile==2 && p) { snprintf(value,sizeof value,"SNR %+.1f",(double)p->snr); text(x,25,kFieldGreen,value); }
    }
    box(4,36,312,40,panel);
    text(8,39,kMutedSlate,"RECEIVED FRAME CADENCE / 30s");
    d.drawLine(12,65-top,308,65-top,kDimGreen);
    for(const auto &packet:packets) {
        const uint32_t age=now-packet.received_ms;
        if(age>=30000) continue;
        const int x=12+static_cast<uint64_t>(30000-age)*294/30000;
        d.drawLine(x,54-top,x,65-top,&packet==p ? kSignalPink : kFieldGreen);
    }
    text(12,67,kMutedSlate,"-30s"); text(288,67,kMutedSlate,"NOW");
    box(4,79,312,27,panel);
    if(p) {
        if(p->framing==model::Framing::Unknown) snprintf(value,sizeof value,"UNKNOWN / UNCLASSIFIED");
        else snprintf(value,sizeof value,"%s? / STRUCTURAL GUESS",model::framingName(p->framing));
        text(8,82,p->framing==model::Framing::Unknown ? kMutedSlate : kCalibrationYellow,value);
        snprintf(value,sizeof value,"FRAME %u/%u / AGE %lus",(unsigned)(cursor+1),(unsigned)packets.size(),(unsigned long)((now-p->received_ms)/1000));
        text(8,95,kMutedSlate,value);
    } else {
        text(8,82,kCalibrationYellow,m.active() ? "LISTENING FOR RECEIVED FRAMES" : "CONFIGURE / START ON DECK");
        text(8,95,kMutedSlate,"NO FRAME SELECTED");
    }
    box(4,109,312,104,panel);
    text(8,112,kMutedSlate,"OFF");
    text(44,112,kMutedSlate,"RAW HEX");
    text(218,112,kMutedSlate,"ASCII");
    if(p) {
        for(size_t row=0;row<8 && row*8<p->len;++row) {
            const auto bytes=model::payloadLine(p->hex,row*8);
            const int y=125+row*11;
            snprintf(value,sizeof value,"%04X",(unsigned)(row*8));
            text(8,y,kDimGreen,value); text(44,y,kPaperPhosphor,bytes.hex.c_str());
            text(218,y,kMutedSlate,bytes.ascii.c_str());
        }
        if(!p->len) text(8,125,kMutedSlate,"EMPTY RECEIVED PAYLOAD");
    } else text(8,142,kMutedSlate,"RAW BYTES APPEAR AFTER A FRAME ARRIVES");
    box(4,217,224,20,panel);
    if(p) snprintf(value,sizeof value,"RAW %u/%u B / NO PAYLOAD DECODE",(unsigned)std::min<uint16_t>(p->len,64),p->len);
    else snprintf(value,sizeof value,"RX ONLY / ZERO TRANSMIT");
    text(8,220,kMutedSlate,value);
    text(8,229,m.active() ? kFieldGreen : kCalibrationYellow,m.active() ? "LISTENING / SELECT ON DECK" : "CACHED / RECEIVER STOPPED");
}
}
