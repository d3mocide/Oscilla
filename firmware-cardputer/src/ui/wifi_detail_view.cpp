/* Paired AP dossier: capture provenance and explicitly cached client links. */
#include "ui/wifi_detail_view.h"
#include "ui/canvas.h"
#include "ui/display_text.h"
#include "ui/external_radar.h"
#include "ui/observatory_chrome.h"
#include "model/contact_inspection.h"
namespace ui {
namespace {
struct Beacon {bool known=false,rsn=false,cap=false,required=false;uint16_t interval=0;};
Beacon beacon(const model::WifiInspection &p){
    const auto &i=p.inspect();const auto &r=p.row();
    if(i.valid&&i.beacons)return {true,i.rsn,i.mfp_capable,i.mfp_required,i.interval_ms};
    return {r.beacon_info,r.rsn,r.mfp_capable,r.mfp_required,r.interval_ms};
}
const char *mfp(const Beacon &b){return !b.known?"UNKNOWN":!b.rsn?"NO RSN":b.required?"REQUIRED":b.cap?"CAPABLE":"NOT OBSERVED";}
std::string name(const std::string &s,size_t n){auto out=printable(s,n);if(s.size()>n&&n>=3)out.replace(n-3,3,"...");return out;}
}
void drawWifiDetailView(const model::WifiInspection &p,const model::SurveyState &scan,const model::ContactsModel &contacts,bool listening,uint32_t now,const ChromeState &chrome)
{
    beginChrome(chrome);auto &d=canvas();const auto &r=p.row();const auto b=beacon(p);
    if(!p.active()){d.setTextSize(1);d.setCursor(4,kBodyTop+20);d.print("NO AP SELECTED");endChrome(chrome);return;}
    instrument::Canvas c(d,0);
    d.fillRect(0,kBodyTop,d.width(),20,kPanelDark);
    d.fillRect(0,kBodyTop,2,20,kCalibrationYellow);
    d.drawFastHLine(0,kBodyTop+20,d.width(),kLineBorder);
    observatory::obsText(c,6,kBodyTop+4,r.ssid.empty()?"<HIDDEN>":r.ssid,observatory::FontId::Title,kCalibrationYellow,230);
    d.setTextSize(1);d.setTextColor(kPaperPhosphor,kVoidInk);d.setCursor(4,kBodyTop+24);d.print(printable(r.bssid,17).c_str());
    if(p.clientsPage()){
        d.setCursor(4,kBodyTop+37);d.setTextColor(kMutedSlate,kVoidInk);
        if(contacts.hasClientSnapshot())d.printf("CLIENT CACHE / %lus OLD",(unsigned long)((now-contacts.clientSnapshotMs())/1000));
        else d.print("NO CLIENT SNAPSHOT");
        const auto *peer=p.peer(contacts);d.setCursor(4,kBodyTop+50);d.setTextColor(kPaperPhosphor,kVoidInk);
        if(peer){d.print(printable(peer->mac,17).c_str());d.setCursor(4,kBodyTop+64);d.printf("%ddBm / CH%u / PKTS%lu",peer->rssi,peer->ch,(unsigned long)peer->pkts);}
        else d.print("NO MATCHING CACHED LINKS");
        d.setCursor(4,kBodyTop+78);d.setTextColor(kMutedSlate,kVoidInk);d.printf("%u OBSERVED AP/CLIENT LINKS",(unsigned)p.peerCount(contacts));
        d.setCursor(4,kBodyTop+91);d.setTextColor(kCalibrationYellow,kVoidInk);d.print("; . PEER / x RADIO / ` LIST");
    }else{
        d.setCursor(4,kBodyTop+37);d.printf("CH%u %s %ddBm / AGE%lus",r.ch,r.band5?"5G":"2.4G",p.signal(),(unsigned long)((now-p.observedMs())/1000));
        d.setCursor(4,kBodyTop+50);
        if(p.inspect().valid&&p.inspect().beacons&&!p.inspect().akm.empty())
            d.printf("SEC %s (%s)",name(r.auth,12).c_str(),name(p.inspect().akm,14).c_str());
        else d.printf("SECURITY %s",name(r.auth,24).c_str());
        d.setCursor(4,kBodyTop+64);
        if(p.inspect().valid&&p.inspect().beacons&&p.inspect().width_mhz){
            const auto &in=p.inspect();
            char phy[32];
            std::snprintf(phy,sizeof phy,"%s%uM%s%s",
                          in.wifi_gen==6?"W6 ":in.wifi_gen==5?"W5 ":in.wifi_gen==4?"W4 ":"",
                          (unsigned)in.width_mhz,
                          in.country.empty()?"":(std::string(" ")+in.country).c_str(),
                          in.wps?" WPS":"");
            d.printf("MFP %s / %s",mfp(b),phy);
        }else d.printf("MFP %s",mfp(b));
        d.setCursor(4,kBodyTop+78);d.setTextColor(kMutedSlate,kVoidInk);
        if(listening)d.print("PASSIVE BEACON CAPTURE...");
        else if(p.inspect().valid)d.printf("%u BEACONS / CAPTURE %lus",p.inspect().beacons,(unsigned long)((now-p.captureMs())/1000));
        else d.print(b.known?"FLAGS / DISCOVERY SNAPSHOT":"NO BEACON CAPTURE");
        d.setCursor(4,kBodyTop+91);d.setTextColor(kCalibrationYellow,kVoidInk);
        d.print(scan.inspectable()&&p.present()?"x CLIENTS / r BEACONS / ` LIST":"x CLIENTS / r SURVEY / ` LIST");
    }
    endChrome(chrome);
}
void drawExternalWifiDetailStrip(lgfx::LGFX_Sprite &d, int top, const model::WifiInspection &p,
                                 const model::SurveyState &scan, const model::ContactsModel &contacts,
                                 bool listening, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const auto &r = p.row();
    const auto b = beacon(p);
    const char *st = listening ? "CAPTURE" : p.stateLabel(scan, now);
    const BadgeTone tone = !p.active() ? BadgeTone::Cached
                         : listening ? BadgeTone::Warn
                         : p.present() ? BadgeTone::Live : BadgeTone::Cached;

    obsFrame(c, ModeSigil::HexOrbit,
             p.clientsPage() ? "AP // CLIENT DOSSIER" : "AP // RADIO DOSSIER",
             st, tone);
    obsCard(c, 164, 36, 140, 103, p.active() ? kCalibrationYellow : kMutedSlate);
    obsDock(c, tone == BadgeTone::Live ? kFieldGreen : kCalibrationYellow, true);

    if (!p.active()) {
        obsRadar(c, 84, 88);
        obsMicro(c, 172, 42, "AP DOSSIER", kMutedSlate, kPanelDark);
        obsText(c, 172, 60, "NO AP", FontId::Title, kCalibrationYellow, 124);
        obsText(c, 12, 164, "NO AP SELECTED", FontId::Title, kPaperPhosphor, 296);
        obsBay(c, 12, 94, "RSSI / dBm", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "CHANNEL", "--", kPaperPhosphor);
        obsBay(c, 224, 84, "AGE / s", "--", kMutedSlate);
        return;
    }

    obsRadar(c, 84, 88);
    const auto *peer = p.peer(contacts);
    const model::ContactKey key{false, peer != nullptr, peer ? peer->mac : "", r.bssid};
    const auto graph = model::contactGraph(contacts, key, false);
    for (size_t i = graph.size; i > 0; --i) {
        const auto &row = contacts.clients()[graph.indices[i - 1]];
        const auto node = radarPoint(row.mac, row.rssi, 84, 88);
        const bool selected = p.clientsPage() && peer && row.mac == peer->mac;
        c.line(84, 88, node.x, node.y, selected ? kSignalPink : kDimGreen);
        obsRadarNode(c, node, selected, false, !p.present());
    }
    c.rect(78, 82, 13, 13, kVoidInk);
    obsRadarNode(c, {84, 88}, !p.clientsPage(), true, !p.present());

    if (p.clientsPage()) {
        obsMicro(c, 172, 42, "SELECTED AP", kMutedSlate, kPanelDark);
        obsText(c, 172, 54, r.ssid.empty() ? "<HIDDEN>" : r.ssid, FontId::Mono, kPaperPhosphor, 124);
        obsMicro(c, 172, 68, r.bssid, kCalibrationYellow, kPanelDark);
        c.line(172, 79, 296, 79, kLineBorder);
        obsText(c, 172, 85, std::to_string(p.peerCount(contacts)) + " LINKS", FontId::Title, kFieldGreen, 124);
    } else {
        obsMicro(c, 172, 42, "SECURITY", kMutedSlate, kPanelDark);
        obsText(c, 172, 54, r.auth.empty() ? "OPEN" : safe(r.auth, 12), FontId::Title, kPaperPhosphor, 124);
        c.line(172, 74, 296, 74, kLineBorder);
        obsMicro(c, 172, 80, std::string("MFP / ") + mfp(b), b.known ? kFieldGreen : kMutedSlate, kPanelDark);
        obsMicro(c, 172, 94,
                 p.inspect().valid ? (std::to_string(p.inspect().beacons) + " BEACONS")
                                   : (b.known ? "DISCOVERY FLAGS" : "NO BEACON DATA"),
                 kPaperPhosphor, kPanelDark);
    }
    const uint32_t beacon_age = now - (p.inspect().valid && p.inspect().beacons ? p.captureMs() : r.observed_ms);
    obsMicro(c, 172, 109,
             b.known ? (std::to_string(b.interval) + "ms / " + std::to_string(beacon_age / 1000) + "s OLD")
                     : "INTERVAL / --",
             kMutedSlate, kPanelDark);
    obsMicro(c, 172, 123,
             contacts.hasClientSnapshot()
                 ? ("CACHE " + std::to_string((now - contacts.clientSnapshotMs()) / 1000) + "s / " + std::to_string(graph.size))
                 : "NO CLIENT CACHE",
             kMutedSlate, kPanelDark);

    /* Lower Reading Dock. */
    obsText(c, 12, 164,
            p.clientsPage() ? (peer ? peer->mac : "NO CACHED CLIENTS")
                            : (r.ssid.empty() ? "<HIDDEN>" : r.ssid),
            FontId::Title, kPaperPhosphor, 296);
    obsText(c, 12, 184, p.clientsPage() ? ("AP " + r.bssid) : r.bssid, FontId::Mono, kMutedSlate, 188);
    obsMicro(c, 308, 186, "NO BEARING", kMutedSlate, kPanelDark, Align::Right);

    obsBay(c, 12, 94, "RSSI / dBm",
           p.clientsPage() ? (peer ? std::to_string(peer->rssi) : "--") : std::to_string(p.signal()),
           kCalibrationYellow);
    obsBay(c, 122, 86, p.clientsPage() ? "PACKETS" : "CHANNEL",
           p.clientsPage() ? (peer ? std::to_string(peer->pkts) : "--") : std::to_string(r.ch),
           kPaperPhosphor);
    obsBay(c, 224, 84, "AGE / s",
           p.clientsPage()
               ? (contacts.hasClientSnapshot() ? std::to_string((now - contacts.clientSnapshotMs()) / 1000) : "--")
               : std::to_string((now - p.observedMs()) / 1000),
           kMutedSlate, "s");
}
}
