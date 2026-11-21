#include "model/contact_inspection.h"
#include "ocp.h"
#include <cassert>
#include <iostream>
static ocp::Item rows(const char *tag,std::vector<std::string> data){ocp::Item f;f.kind=ocp::ItemKind::Frame;f.tag=tag;f.rows=data;return f;}
static std::string client(const std::string &ap,const std::string &mac,unsigned n){return ap+","+mac+",6,2.4,-54,"+std::to_string(n);}
int main(){
    model::ContactsModel m;m.begin();
    const std::string a="02:00:00:00:00:01",b="02:00:00:00:00:02",c="02:00:00:00:00:03";
    m.absorbClients(rows(OCP_MARK_CLIENTS,{client(a,b,1),client(c,b,2)}),1000);
    auto key=model::contactKey(m,false,1);model::ContactInspection detail;
    assert(detail.open(m,false,1));assert(detail.client().bssid==c);
    m.absorbClients(rows(OCP_MARK_CLIENTS,{client(c,b,9),client(a,b,3)}),2000);
    detail.refresh(m);assert(detail.present()&&detail.client().pkts==9&&detail.sampleMs()==2000);
    assert(model::contactCursor(m,key,1)==0);
    assert(std::string(detail.stateLabel(true,7500))=="AGED");
    assert(std::string(detail.stateLabel(false,2500))=="CACHED");
    auto links=model::contactGraph(m,key,true);assert(links.total==2&&links.indices[0]==0);
    m.absorbClients(rows(OCP_MARK_CLIENTS,{client(a,b,4)}),3000);
    detail.refresh(m);assert(!detail.present()&&detail.client().bssid==c&&detail.client().pkts==9);
    assert(detail.sampleMs()==2000&&model::contactCursor(m,key,1)==0);
    m.absorbClients(rows(OCP_MARK_CLIENTS,{}),4000);detail.refresh(m);assert(!detail.present());
    assert(model::contactCursor(m,key,99)==0);
    assert(!detail.open(m,false,99)&&!detail.active());
    std::vector<std::string> peers;for(unsigned i=0;i<30;++i){char mac[24];snprintf(mac,sizeof mac,"02:00:00:00:01:%02x",i);peers.push_back(client(a,mac,i));}
    m.absorbClients(rows(OCP_MARK_CLIENTS,peers),5000);auto selected=model::contactKey(m,false,29);
    links=model::contactGraph(m,selected,false);assert(links.size==12&&links.total==30&&links.indices[0]==29);
    m.absorbProbes(rows(OCP_MARK_PROBES,{b+",\"\",-61,3",b+",\"lab\",-63,4"}),6000);
    assert(detail.open(m,true,0)&&detail.request().ssid.empty());
    auto request=detail.key();assert(request.probes&&model::contactCursor(m,request,1)==0);
    assert(model::contactGraph(m,request,true).total==2);
    auto absent=model::contactKey(m,true,100);assert(absent.probes&&!absent.valid);
    model::ApRow wrong;wrong.bssid=c;wrong.ssid="WRONG";
    assert(detail.open(m,false,0,&wrong)&&detail.cachedApName().empty());
    model::ApRow cached;cached.bssid=a;cached.ssid=std::string("lab\x1b",4);
    assert(detail.open(m,false,0,&cached)&&detail.cachedApName()==cached.ssid);
    detail.close();assert(!detail.active()&&std::string(detail.stateLabel(true,10000))=="EMPTY");
    std::cout<<"contact inspection: identity, reorder, disappearing rows, selection bounds, wildcard and graph cap passed\n";
}
