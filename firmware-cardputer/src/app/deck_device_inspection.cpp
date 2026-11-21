/* AP/advertiser inspection entry, identity remapping and explicit refresh. */
#include "app/deck_app.h"
namespace app {
void DeckApp::openWifiDossier()
{
    if(!wifi_inspection_.open(scan_,cursor_)){notice("no AP selected");return;}
    screen_=Screen::Trace;dirty_=true;
}
void DeckApp::openBtDossier()
{
    if(!bt_inspection_.open(bt_,bt_cursor_)){notice("no advertiser selected");return;}
    dirty_=true;
}
uint16_t DeckApp::absorbWifiPage(const ocp::Item &item)
{
    const auto key=wifi_inspection_.active()?wifi_inspection_.row().bssid:ap_focus_bssid_;
    const auto next=scan_.absorbPage(item,now_);wifi_inspection_.refresh(scan_);
    const auto i=model::apIndex(scan_,key);
    cursor_=i<scan_.rows().size()?i:(scan_.rows().empty()?0:std::min(cursor_,scan_.rows().size()-1));
    return next;
}
void DeckApp::absorbBtSnapshot(const ocp::Item &item)
{
    const auto key=bt_inspection_.active()?bt_inspection_.row().mac:bt_focus_mac_;
    bt_.absorbScan(item,now_);bt_inspection_.refresh(bt_);
    const auto i=model::btIndex(bt_,key);
    bt_cursor_=i<bt_.devices().size()?i:0;
}
void DeckApp::absorbDeviceEvent(const ocp::Item &item)
{
    const auto *kind=item.get(OCP_K_KIND);if(!kind)return;
    if(*kind==OCP_EVT_KIND_NETWORK){
        scan_.absorbEvent(item,now_);wifi_inspection_.refresh(scan_);
    }else{
        bt_.absorbEvent(item,now_);bt_inspection_.refresh(bt_);
    }
}
void DeckApp::refreshWifiDossier(uint32_t now)
{
    if(scan_.scanning()||traceListening()){notice("capture in progress");return;}
    if(wifi_inspection_.present()&&scan_.inspectable()&&!scan_.continuousActive()){startInspect(now);return;}
    if(!preparePhyStart([this](uint32_t t){refreshWifiDossier(t);},now))return;
    requestScan(now);dirty_=true;
}
void DeckApp::refreshBtDossier(uint32_t now)
{
    if(bt_.scanning()||bt_scan_pending_){notice("scan in progress");return;}
    if(!preparePhyStart([this](uint32_t t){refreshBtDossier(t);},now))return;
    startBtScan(now);dirty_=true;
}
void DeckApp::startInspect(uint32_t now)
{
    if(!wifi_inspection_.active())openWifiDossier();
    if(!wifi_inspection_.active())return;
    const auto i=model::apIndex(scan_,wifi_inspection_.row().bssid);
    if(i==scan_.rows().size()){notice("AP missing; refresh survey first");return;}
    if(scan_.continuousActive()||!scan_.inspectable()){notice("r snapshot survey first");return;}
    if(client_.state()!=ocp::LinkState::Ready){notice("no probe");return;}
    if(!preparePhyStart([this](uint32_t t){startInspect(t);},now))return;
    trace_idx_=scan_.rows()[i].idx;
    if(!client_.send(std::string(OCP_V_INSPECT_NETWORK)+" "+std::to_string(trace_idx_),now)){
        retrySoon([this](uint32_t t){startInspect(t);},now);return;
    }
    screen_=Screen::Trace;notice("");
}
}
