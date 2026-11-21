/* Stable inspection snapshots, independent of live receiver buffers. */
#pragma once
#include "model/lora_model.h"
#include "model/lora_payload_view.h"
#include "model/spectrum_model.h"
namespace model {
class PacketInspection {
    LoraPacket packet_;
    size_t offset_=0;
    bool active_=false;
public:
    void open(const LoraPacket &packet) {
        close();
        if(packet.hex.size()%2 || packet.hex.size()>510 || packet.hex.size()/2!=packet.len)return;
        for(char c:packet.hex)if(hexNibble(c)<0)return;
        packet_=packet;active_=true;
    }
    void close(){active_=false;offset_=0;}
    bool active()const{return active_;}
    const LoraPacket &packet()const{return packet_;}
    size_t offset()const{return offset_;}
    void page(int direction){
        if(direction<0)offset_=offset_>=64?offset_-64:0;
        else if(offset_+64<packet_.hex.size()/2)offset_+=64;
    }
    PayloadLine line(size_t row)const{return row<8?payloadLine(packet_.hex,offset_+row*8):PayloadLine{};}
};
class ScopeInspection {
    SpectrumModel parent_;
    size_t cursor_=0;
    bool saved_=false;
public:
    void save(const SpectrumModel &m,size_t cursor){
        if(m.locked())return;
        parent_=m;cursor_=cursor;saved_=true;
    }
    bool restore(SpectrumModel &m,size_t &cursor){
        if(!saved_)return false;
        m=parent_;m.stop();cursor=cursor_;saved_=false;return true;
    }
};
}
