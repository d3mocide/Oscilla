/* Identity-pinned AP/advertiser dossiers; radio commands remain in the app. */
#pragma once
#include "model/scan_model.h"
#include "model/bt_model.h"
#include "model/contacts_model.h"
#include "model/signal_history.h"
namespace model {
inline size_t apIndex(const ScanModel &m,const std::string &bssid){
    for(size_t i=0;i<m.rows().size();++i)if(m.rows()[i].bssid==bssid)return i;
    return m.rows().size();
}
inline size_t btIndex(const BtModel &m,const std::string &mac){
    for(size_t i=0;i<m.devices().size();++i)if(m.devices()[i].mac==mac)return i;
    return m.devices().size();
}
/* Only receiver flags are copied into the AP viewport, not its inventory. */
class SurveyState {
    bool scanning_=false,continuous_=false,inspectable_=false;
public:
    SurveyState()=default;
    SurveyState(const ScanModel &m):scanning_(m.scanning()),continuous_(m.continuousActive()),inspectable_(m.inspectable()){}
    bool scanning()const{return scanning_;}bool continuousActive()const{return continuous_;}bool inspectable()const{return inspectable_;}
};
class WifiInspection {
    ApRow row_;Inspect inspect_;
    bool active_=false,present_=false,clients_=false;
    std::string peer_;
    int signal_=0;uint32_t observed_ms_=0,capture_ms_=0;
public:
    bool open(const ScanModel &m,size_t cursor){
        close();if(cursor>=m.rows().size())return false;
        row_=m.rows()[cursor];active_=present_=true;signal_=row_.rssi;observed_ms_=row_.observed_ms;return true;
    }
    void refresh(const ScanModel &m){
        if(!active_)return;
        const auto i=apIndex(m,row_.bssid);present_=i<m.rows().size();if(!present_)return;
        if(row_.observation_id!=m.rows()[i].observation_id){signal_=m.rows()[i].rssi;observed_ms_=m.rows()[i].observed_ms;}
        row_=m.rows()[i];
    }
    bool accept(const Inspect &in,uint32_t now){
        if(!active_||!in.valid||in.bssid!=row_.bssid)return false;
        inspect_=in;capture_ms_=now;
        if(in.beacons){signal_=in.rssi;observed_ms_=now;}return true;
    }
    void close(){active_=present_=clients_=false;inspect_={};peer_.clear();}
    bool active()const{return active_;}bool present()const{return present_;}
    const ApRow &row()const{return row_;}const Inspect &inspect()const{return inspect_;}
    int signal()const{return signal_;}uint32_t observedMs()const{return observed_ms_;}uint32_t captureMs()const{return capture_ms_;}
    bool clientsPage()const{return clients_;}void togglePage(){clients_=!clients_;}
    size_t peerCount(const ContactsModel &m)const{size_t n=0;for(const auto &r:m.clients())if(r.bssid==row_.bssid)++n;return n;}
    const ClientRow *peer(const ContactsModel &m)const{
        const ClientRow *first=nullptr;for(const auto &r:m.clients())if(r.bssid==row_.bssid){if(!first)first=&r;if(r.mac==peer_)return &r;}return first;
    }
    void movePeer(const ContactsModel &m,int direction){
        const auto *selected=peer(m);if(!selected)return;
        size_t current=0,count=0;for(const auto &r:m.clients())if(r.bssid==row_.bssid){if(r.mac==selected->mac)current=count;++count;}
        const size_t target=direction<0?(current?current-1:0):std::min(current+1,count-1);
        count=0;for(const auto &r:m.clients())if(r.bssid==row_.bssid&&count++==target){peer_=r.mac;break;}
    }
    const char *stateLabel(const SurveyState &m,uint32_t now)const{
        if(!active_)return "EMPTY";
        if(m.scanning())return "SURVEY";
        if(!present_)return "ROW MISSING";
        if(!m.continuousActive())return "CACHED";
        return now-observed_ms_<5000?"OBSERVED":"AGED";
    }
};
class BtInspection {
    BtDevice row_;SignalHistory history_;bool active_=false,present_=false;
public:
    bool open(const BtModel &m,size_t cursor){
        close();if(cursor>=m.devices().size())return false;
        row_=m.devices()[cursor];active_=present_=true;history_.push(row_.observed_ms,row_.rssi);return true;
    }
    void refresh(const BtModel &m){
        if(!active_)return;
        const auto i=btIndex(m,row_.mac);present_=i<m.devices().size();if(!present_)return;
        if(row_.observation_id!=m.devices()[i].observation_id)history_.push(m.devices()[i].observed_ms,m.devices()[i].rssi);
        row_=m.devices()[i];
    }
    void close(){active_=present_=false;history_={};}
    bool active()const{return active_;}bool present()const{return present_;}
    const BtDevice &row()const{return row_;}const SignalHistory &history()const{return history_;}
    const char *stateLabel(const BtModel &m,uint32_t now)const{
        if(!active_)return "EMPTY";
        if(m.scanning())return "SCANNING";
        if(!present_)return "ROW MISSING";
        if(!m.continuousActive()&&!m.airtagActive())return "CACHED";
        return now-row_.observed_ms<5000?"OBSERVED":"AGED";
    }
};
}
