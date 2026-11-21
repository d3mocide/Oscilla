#include "model/inspection_state.h"
#include <cassert>
#include <iostream>
int main() {
    model::PacketInspection p;
    model::LoraPacket source; source.hex=std::string(510,'A');source.len=255;source.rssi=-71;
    p.open(source);source.hex="00";source.rssi=-1;
    assert(p.packet().hex.size()==510 && p.packet().rssi==-71);
    for(int i=0;i<100;i++)p.page(1);
    assert(p.offset()==192 && p.line(7).bytes==7);
    p.page(-1);assert(p.offset()==128);
    for(int i=0;i<100;i++)p.page(-1);
    assert(p.offset()==0);p.close();assert(!p.active());
    source.hex="00X1";p.open(source);assert(!p.active());
    source.hex="001";p.open(source);assert(!p.active());
    model::ScopeInspection scope;model::SpectrumModel survey;
    survey.begin();
    ocp::Item event;event.kind=ocp::ItemKind::Event;
    event.kv={{"kind","chan"},{"ch","6"},{"pkts","42"}};survey.absorbEvent(event,1234);
    scope.save(survey,0);survey.beginLocked(6);
    scope.save(survey,0);size_t cursor=0;
    assert(scope.restore(survey,cursor));assert(!survey.locked()&&!survey.active()&&cursor==0);
    assert(survey.readings().size()==1 && survey.readings()[0].pkts==42 && survey.readings()[0].received_ms==1234);
    assert(!scope.restore(survey,cursor));
    std::cout<<"inspection state: immutable payload, malformed input, paging, parent restore passed\n";
}
