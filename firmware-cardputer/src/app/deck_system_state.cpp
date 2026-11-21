/* Resource-view snapshots and navigation; deck keeps receiver ownership. */
#include "app/deck_app.h"
#include "storage/session_reader.h"
namespace app {
void DeckApp::navigateCard(Screen target,uint32_t now)
{
    if(target==screen_)return;
    if(system_navigation_.preserves(target)) {
        if(isInspectionScreen(target))system_navigation_.enter(screen_);
    } else stopPhyForNavigation(now);
    if(screen_==Screen::Logs&&!isSystemScreen(target))storage::sessionCancel(session_review_);
    if(!isSystemScreen(target))system_from_sessions_=false;
    screen_=target;
    if(target==Screen::Logs&&session_review_.catalog.entries.empty())storage::sessionCatalog(session_review_);
    if(!isInspectionScreen(target)&&target!=Screen::Trace)wifi_inspection_.close();
    if(!isInspectionScreen(target)&&target!=Screen::Beacons)bt_inspection_.close();
    if(!isInspectionScreen(target)&&target!=Screen::Contacts)contact_inspection_.close();
    if(!isInspectionScreen(target)){system_navigation_.clear();packet_inspection_.close();}
    dirty_=true;
}
model::SystemStatus DeckApp::externalSystemState()const
{
    model::SystemStatus s;
    s.info=showingInfo();s.link=client_.state();s.probe=client_.probe();s.stats=client_.stats();
    s.probe_valid=probe_status_valid_;s.probe_heap=probe_heap_;s.probe_total=probe_heap_total_;
    s.probe_uptime=probe_uptime_ms_;s.probe_sample_ms=last_status_reply_ms_;s.probe_sequence=probe_status_sequence_;s.probe_epoch=probe_status_epoch_;
    s.probe_owner=probe_owner_;s.probe_lora=probe_lora_;
    if(s.probe_owner==OCP_OWNER_WIFI){
        if(spectrum_.active())s.phy_task=spectrum_.locked()?"CHANNEL "+std::to_string(spectrum_.lockedChannel()):"CHANNEL SURVEY";
        else if(contacts_.sniffing())s.phy_task="SNIFFER";
        else if(deauth_.active())s.phy_task="DEAUTH WATCH";
        else if(scan_.scanning()||scan_.continuousActive())s.phy_task="WI-FI SCAN";
        else s.phy_task="WI-FI";
    } else if(s.probe_owner==OCP_OWNER_BLE){
        s.phy_task=anti_.active()?"ANTI-SURVEILLANCE":bt_.airtagActive()?"TRACKER WATCH":"BLE SCAN";
    } else if(s.probe_owner==OCP_OWNER_IEEE802154)s.phy_task="802.15.4 RECON";
    else if(s.probe_owner==OCP_OWNER_NONE)s.phy_task="IDLE";
    else s.phy_task="UNKNOWN";
    return s;
}
}
