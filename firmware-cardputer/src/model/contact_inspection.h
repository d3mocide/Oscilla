/* Identity-pinned sniffer inspection; latest snapshots remain authoritative. */
#pragma once
#include <array>
#include <algorithm>
#include "model/contacts_model.h"
#include "model/scan_model.h"
namespace model {
struct ContactKey {bool probes=false,valid=false;std::string mac,target;};
inline ContactKey contactKey(const ContactsModel &m,bool probes,size_t cursor){
    if(probes){if(cursor>=m.probes().size())return {true,false,{},{}};const auto &r=m.probes()[cursor];return {true,true,r.mac,r.ssid};}
    if(cursor>=m.clients().size())return {false,false,{},{}};
    const auto &r=m.clients()[cursor];return {false,true,r.mac,r.bssid};
}
inline size_t contactIndex(const ContactsModel &m,const ContactKey &key){
    const size_t count=key.probes?m.probes().size():m.clients().size();
    if(key.valid)for(size_t i=0;i<count;++i){
        const auto &mac=key.probes?m.probes()[i].mac:m.clients()[i].mac;
        const auto &target=key.probes?m.probes()[i].ssid:m.clients()[i].bssid;
        if(mac==key.mac&&target==key.target)return i;
    }
    return count;
}
inline size_t contactCursor(const ContactsModel &m,const ContactKey &key,size_t fallback){
    const size_t count=key.probes?m.probes().size():m.clients().size(),found=contactIndex(m,key);
    return found<count?found:count?std::min(fallback,count-1):0;
}
struct ContactGraph {std::array<size_t,12> indices{};size_t size=0,total=0;};
inline ContactGraph contactGraph(const ContactsModel &m,const ContactKey &key,bool by_device){
    ContactGraph out;if(!key.valid)return out;
    const size_t count=key.probes?m.probes().size():m.clients().size(),selected=contactIndex(m,key);
    if(selected<count)out.indices[out.size++]=selected;
    for(size_t i=0;i<count;++i){
        const auto &value=by_device?(key.probes?m.probes()[i].mac:m.clients()[i].mac):
                                    (key.probes?m.probes()[i].ssid:m.clients()[i].bssid);
        if(value==(by_device?key.mac:key.target)){
            ++out.total;if(i!=selected&&out.size<out.indices.size())out.indices[out.size++]=i;
        }
    }
    return out;
}
class ContactInspection {
    ContactKey key_;
    ClientRow client_;
    ProbeRow request_;
    uint32_t sample_ms_=0;
    bool present_=false;
    std::string ap_name_;
public:
    bool open(const ContactsModel &m,bool probes,size_t cursor,const ApRow *ap=nullptr){
        close();key_=contactKey(m,probes,cursor);if(!key_.valid)return false;
        if(ap&&!probes&&ap->bssid==key_.target)ap_name_=ap->ssid;
        refresh(m);return true;
    }
    void refresh(const ContactsModel &m){
        if(!active())return;
        const size_t i=contactIndex(m,key_),count=key_.probes?m.probes().size():m.clients().size();
        present_=i<count;if(!present_)return;
        if(key_.probes){request_=m.probes()[i];sample_ms_=m.probeSnapshotMs();}
        else{client_=m.clients()[i];sample_ms_=m.clientSnapshotMs();}
    }
    void close(){key_={};present_=false;ap_name_.clear();}
    const char *stateLabel(bool live,uint32_t now)const{
        if(!active())return "EMPTY";
        if(!present_)return "ROW MISSING";
        if(!live)return "CACHED";
        return now-sample_ms_<5000?"LIVE":"AGED";
    }
    bool active()const{return key_.valid;}
    bool present()const{return present_;}
    const ContactKey &key()const{return key_;}
    const ClientRow &client()const{return client_;}
    const ProbeRow &request()const{return request_;}
    const std::string &cachedApName()const{return ap_name_;}
    uint32_t sampleMs()const{return sample_ms_;}
    int rssi()const{return key_.probes?request_.rssi:client_.rssi;}
    uint32_t packets()const{return key_.probes?request_.pkts:client_.pkts;}
};
}
