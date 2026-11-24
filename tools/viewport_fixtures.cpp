/* Synthetic source-render fixtures; never opens a radio or writes field data. */
#include <M5GFX.h>
#include <functional>
#include <fstream>
#include <string>
#include <cstdio>
#include "ocp.h"
#include "ui/external_spectrum_view.h"
#include "ui/external_wifi_view.h"
#include "ui/external_bt_view.h"
#include "ui/external_contacts_view.h"
#include "ui/external_contact_detail_view.h"
#include "ui/canvas.h"
#include "ui/contacts_view.h"
#include "ui/contact_detail_view.h"
#include "ui/wifi_detail_view.h"
#include "ui/bt_detail_view.h"
#include "ui/sweep_view.h"
#include "ui/bt_view.h"
#include "ui/deauth_view.h"
#include "ui/zig_view.h"
#include "ui/anti_surveillance_view.h"
#include "ui/subghz_view.h"
#include "ui/ook_view.h"
#include "ui/spectrum_view.h"
#include "ui/session_view.h"
#include "ui/external_lora_view.h"
#include "ui/external_zig_view.h"
#include "ui/external_drive_view.h"
#include "ui/external_system_view.h"
#include "ui/external_card_views.h"
#include "ui/external_hunt_view.h"
#include "ui/external_gnss_view.h"
#include "ui/external_ook_view.h"
#include "ui/gnss_fix_view.h"
#include "ui/settings_view.h"
#include "ui/signal_hunt_view.h"

static ocp::Item event(std::initializer_list<ocp::KeyValue> fields) {
    ocp::Item item;item.kind=ocp::ItemKind::Event;item.kv=fields;return item;
}
static ocp::Item frame(const char *tag,std::vector<std::string> rows) {
    ocp::Item item;item.kind=ocp::ItemKind::Frame;item.tag=tag;item.rows=rows;
    item.kv={{"total",std::to_string(rows.size())},{"first","0"},{"n",std::to_string(rows.size())}};return item;
}
static void capture(const std::string &path,const std::function<void(lgfx::LGFX_Sprite &,int)> &render) {
    std::ofstream file(path,std::ios::binary);file<<"P6\n320 240\n255\n";
    lgfx::LGFX_Sprite strip;
    for(int top=0;top<240;top+=24) {
        render(strip,top);
        for(int y=0;y<24;++y)for(int x=0;x<320;++x) {
            uint16_t pixel=strip.readPixel(x,y);char rgb[]={char(((pixel>>11)&31)*255/31),char(((pixel>>5)&63)*255/63),char((pixel&31)*255/31)};
            file.write(rgb,3);
        }
    }
}
static void captureDeck(const std::string &path,const std::function<void()> &render){
    ui::initCanvas();render();auto &d=ui::canvas();
    std::ofstream file(path,std::ios::binary);file<<"P6\n240 135\n255\n";
    for(int y=0;y<135;++y)for(int x=0;x<240;++x){
        auto p=d.readPixel(x,y);char rgb[]={char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};file.write(rgb,3);
    }
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    const std::string dir=argv[1];
    for(int mode=0;mode<3;++mode) {
        const bool empty=mode==1,cached=mode==2;const uint32_t now=30000;
        const std::string suffix=empty?"-empty":cached?"-cached":"-active";
        auto save=[&](const char *name,const std::function<void(lgfx::LGFX_Sprite &,int)> &render){capture(dir+"/"+name+suffix+".ppm",render);};
        model::SessionReview archive;archive.available=!empty;
        if(!empty){
            for(int index=1;index<=8;++index)archive.catalog.add({model::SessionKind::Drive,index,model::Csv|model::Kml,32000});
            archive.catalog.add({model::SessionKind::Lora,7,model::Csv|model::Manifest|model::Health,12000});
            archive.cursor=2;
        }
        auto saveArchive=[&](const char *name){save(name,[&](auto &d,int t){ui::drawExternalSessionStrip(d,t,archive);});
            captureDeck(dir+"/deck-"+name+suffix+".ppm",[&]{ui::ChromeState chrome;chrome.title="SESSIONS";ui::drawSessionView(archive,chrome);});};
        saveArchive("sessions-catalog");
        archive.stage=model::ReviewStage::Summary;archive.selected={model::SessionKind::Lora,7,model::Csv|model::Manifest|model::Health,12000};archive.snapshot_bytes=12000;
        if(!empty){archive.summary.header=true;for(unsigned i=0;i<20;++i){model::SavedRecord row;row.name="RAW PACKET";row.rssi=-60-int(15*std::sin(i*.4));row.packet.rssi=row.rssi;row.packet.snr=4.5;row.packet.len=80;row.packet.hex=std::string(160,'A');row.time=1000+i*2000;row.timed=true;row.stamp=std::to_string(row.time)+"ms";row.frequency=869525000;row.sf=7;row.bw=125;row.cr=1;archive.summary.add(row);if(i<6)archive.rows.push_back(row);}}
        archive.busy=cached;archive.read_bytes=4096;saveArchive("sessions-summary");archive.busy=false;
        archive.stage=model::ReviewStage::Records;archive.record=2;saveArchive("sessions-records");
        if(!empty){archive.packet.open(archive.rows[2].packet);archive.stage=model::ReviewStage::Hex;saveArchive("sessions-hex");}
        archive.stage=model::ReviewStage::Summary;archive.error="CSV MISSING / SIDECARS ONLY";saveArchive("sessions-error");
        model::PacketInspection payload;
        model::LoraPacket raw;raw.rssi=-67;raw.snr=8.5;raw.len=255;raw.hex=std::string(510,'A');
        raw.hex.replace(0,16,"004F5343494C4C41");payload.open(raw);
        save("packet-hex",[&](auto &d,int top){ui::drawExternalLoraDetailStrip(d,top,payload);});
        payload.page(1);payload.page(1);payload.page(1);
        save("packet-hex-last",[&](auto &d,int top){ui::drawExternalLoraDetailStrip(d,top,payload);});
        model::SpectrumModel scope,broad;scope.beginLocked(6);broad.begin();
        model::ScanModel scan;scan.beginContinuous();model::BtModel bt;bt.beginContinuous();
        model::ContactsModel contacts;contacts.begin();model::ZigModel zig;zig.begin();
        model::LoraModel lora;lora.configured(915000000,7,125,1,0x12,"fixture");lora.begin();
        model::DeauthModel deauth;deauth.begin();model::AntiSurveillanceModel anti;anti.begin();
        model::GnssModel gps;model::DriveTrail trail;storage::WardriveLogStats log;
        if(!empty) {
            for(unsigned i=0;i<30;++i)scope.absorbEvent(event({{"kind","chan"},{"ch","6"},{"pkts",std::to_string(8+unsigned(22*std::abs(std::sin(i*.6))))}}),1000+i*1000);
            for(int i=0;i<12;++i) {
                char mac[24];snprintf(mac,sizeof mac,"02:00:00:00:00:%02x",i);
                broad.absorbEvent(event({{"kind","chan"},{"ch",std::to_string(i+1)},{"pkts",std::to_string((i*13)%40)}}),now-500);
                scan.absorbEvent(event({{"kind","network"},{"ssid",i==5?"OBSERVATORY / LAB":"Synthetic network "+std::to_string(i)},{"bssid",mac},{"ch",std::to_string(i+1)},{"rssi",std::to_string(-95+(i*11)%60)},{"band","2.4"},{"privacy","1"},{"rsn","1"},{"mfp_capable","1"},{"mfp_required","0"},{"interval_ms","100"}}),now-500);
                bt.absorbEvent(event({{"kind","ble"},{"mac",mac},{"name",i==5?"FIELD SENSOR 06":"Synthetic device "+std::to_string(i)},{"rssi",std::to_string(-95+(i*11)%60)}}),now-500);
                if(i<5)lora.absorbEvent(event({{"kind","lora"},{"rssi","-67"},{"snr","8.5"},{"len","8"},{"hex","0102030405060708"}}),now-i*4500);
                deauth.absorbEvent(event({{"kind","deauth"},{"bssid","02:00:00:00:00:aa"},{"mac",mac},{"reason","7"},{"disassoc","0"},{"rssi","-58"},{"ch",std::to_string((i%8)+1)},{"n","1"}}));
            }
            contacts.absorbClients(frame(OCP_MARK_CLIENTS,{"\"02:00:00:00:00:aa\",\"02:00:00:00:00:01\",\"6\",\"2.4\",\"-54\",\"128\"","\"02:00:00:00:00:aa\",\"02:00:00:00:00:02\",\"6\",\"2.4\",\"-66\",\"30\"","\"02:00:00:00:00:aa\",\"02:00:00:00:00:03\",\"6\",\"2.4\",\"-70\",\"18\""}),now-500);
            contacts.absorbProbes(frame(OCP_MARK_PROBES,{"\"02:00:00:00:00:01\",\"Synthetic SSID\",\"-54\",\"128\"","\"02:00:00:00:00:02\",\"Synthetic SSID\",\"-66\",\"30\""}),now-500);
            zig.absorb(frame(OCP_MARK_ZIG,{"\"pan\",\"1a2b\",\"802154\",\"mac\",\"0001\",\"3\",\"-60\",\"191\"","\"node\",\"1a2b\",\"1234\",\"\",\"unknown\",\"-60\",\"191\",\"10\""}));
            gps.absorb({"GN","GGA",{"120000","4500.000","N","12200.000","W","1","8","0.8","10"}},now);
            model::GnssFix fix;fix.valid=true;fix.lat_deg=45;fix.lon_deg=-122;
            for(unsigned i=0;i<40;++i){fix.lat_deg=45+std::sin(i*.1)*.0006;fix.lon_deg=-122+i*.00004;trail.observe(fix,true,1000+i*500,1000+i*500,true);}
            log.open=true;log.aps=27;
            for(int i=0;i<3;++i){anti.observePosition(45+i*.001,-122,true,0,now-2000+i*500);anti.absorbEvent(event({{"kind","airtag"},{"mac","02:00:00:00:00:01"},{"rssi","-65"},{"n","1"}}),now-2000+i*500);}
        }
        if(cached){scope.stop();broad.stop();scan.stop();bt.stop();contacts.stopSniffing();zig.stop();lora.stop();deauth.stop();anti.stop();}
        save("packet-monitor",[&](auto &d,int t){ui::drawExternalSpectrumStrip(d,t,scope,0,now);});
        save("channel-field",[&](auto &d,int t){ui::drawExternalSpectrumStrip(d,t,broad,5,now);});
        save("wifi",[&](auto &d,int t){ui::drawExternalWifiStrip(d,t,scan,5,now,false);});
        save("bluetooth",[&](auto &d,int t){ui::drawExternalBtStrip(d,t,bt,5);});
        save("contacts",[&](auto &d,int t){ui::drawExternalContactsStrip(d,t,contacts,false,0,now);});
        save("requests",[&](auto &d,int t){ui::drawExternalContactsStrip(d,t,contacts,true,0,now);});
        model::ContactInspection client_detail,request_detail;
        client_detail.open(contacts,false,0);request_detail.open(contacts,true,0);
        save("client-detail",[&](auto &d,int t){ui::drawExternalContactDetailStrip(d,t,contacts,client_detail,now);});
        save("request-detail",[&](auto &d,int t){ui::drawExternalContactDetailStrip(d,t,contacts,request_detail,now);});
        ui::ChromeState chrome;chrome.title="SNIFFER";chrome.link=ocp::LinkState::Ready;chrome.battery_pct=77;chrome.live=!cached;
        auto deckSave=[&](const char *name,const std::function<void()> &render){captureDeck(dir+"/"+name+suffix+".ppm",render);};
        deckSave("deck-clients",[&]{ui::drawContactsView(contacts,ui::ContactsTab::Clients,0,chrome);});
        deckSave("deck-requests",[&]{ui::drawContactsView(contacts,ui::ContactsTab::Probes,0,chrome);});
        chrome.title="WI-FI SCAN";
        deckSave("deck-wifi",[&]{ui::drawSweepView(scan,1,12000,chrome);});
        chrome.title="BLE SCAN";
        deckSave("deck-ble",[&]{ui::drawBtView(bt,1,12000,chrome);});
        chrome.title="DEAUTH DETECT";
        deckSave("deck-deauth",[&]{ui::drawDeauthView(deauth,0,chrome);});
        chrome.title="802.15.4";
        deckSave("deck-zig-pans",[&]{ui::drawZigView(zig,ui::ZigTab::Pans,0,chrome);});
        deckSave("deck-zig-nodes",[&]{ui::drawZigView(zig,ui::ZigTab::Nodes,0,chrome);});
        chrome.title="ANTI-SURV";
        deckSave("deck-anti",[&]{ui::drawAntiSurveillanceView(anti,0,false,chrome);});
        chrome.title="LORA RX";
        deckSave("deck-lora",[&]{ui::drawSubGhzView(lora,1,chrome,"MESHTASTIC US");});
        chrome.title="OOK RX";
        {
            const uint32_t t=now+4000000u;   /* own clock base: hour-old rows must not wrap */
            model::OokModel ook;ook.begin();
            {ocp::Item r;r.kind=ocp::ItemKind::Frame;r.tag="[OOK]";
             r.kv={{"running","1"},{"freq_khz","433920"},{"baud","24993"},{"trains","52"},{"packets","10"},{"unknown","7"},
                   {"overflows","0"},{"dropped","0"},{"errors","0"},{"reinit","0"},{"hw_fault","0"},{"ocp_drop","0"}};
             ook.absorbReply(r);}
            auto prn=[&](const char *hex,const char *rssi,uint32_t at){ook.absorbEvent(event({{"kind","ook"},{"decoder","princeton"},{"nbits","24"},{"hex",hex},{"frames","1"},{"te_us","400"},{"rssi",rssi}}),at);};
            auto lax=[&](const char *id,const char *ch,const char *t,const char *hum,const char *bl,const char *rssi,uint32_t at){
                ook.absorbEvent(event({{"kind","ook"},{"decoder","lacrosse_tx141thbv2"},{"nbits","40"},{"hex","c0824f3d35"},{"frames","1"},{"te_us","240"},{"rssi",rssi},
                                       {"id",id},{"ch",ch},{"batt_low",bl},{"test","0"},{"temp_c10",t},{"hum",hum}}),at);};
            prn("5f0a21","-78",t-3600000);
            lax("71","2","-35","88","0","-83",t-400000);
            prn("a5c3e1","-41",t-300000);prn("a5c3e1","-43",t-200000);
            lax("192","0","91","61","1","-77",t-52000);
            prn("a5c3e1","-40",t-8000);
            ook.absorbEvent(event({{"kind","ook_unknown"},{"n","7"},{"pulses","66"},{"min_us","160"},{"max_us","1240"},{"dur_ms","47"}}),t-2000);
            deckSave("deck-ook",[&]{ui::drawOokView(ook,0,t,chrome);});
            deckSave("deck-ook-sensor",[&]{ui::drawOokView(ook,1,t,chrome);});
            model::OokModel idle;
            deckSave("deck-ook-idle",[&]{ui::drawOokView(idle,0,t,chrome);});
            save("ook",[&](auto &d,int top){ui::drawExternalOokStrip(d,top,ook,0,t);});
            save("ook-sensor",[&](auto &d,int top){ui::drawExternalOokStrip(d,top,ook,1,t);});
            save("ook-idle",[&](auto &d,int top){ui::drawExternalOokStrip(d,top,idle,0,t);});
            auto stopped=ook;stopped.stop();
            save("ook-cached",[&](auto &d,int top){ui::drawExternalOokStrip(d,top,stopped,2,t);});
        }
        chrome.title="GNSS";
        deckSave("deck-gnss",[&]{ui::drawGnssFixView(gps,now,chrome);});
        chrome.title="PKT MON";
        deckSave("deck-spectrum",[&]{ui::drawSpectrumView(broad,1,chrome);});
        chrome.title="CLIENT DETAIL";
        if(client_detail.active())deckSave("deck-client-detail",[&]{ui::drawContactDetailView(client_detail,contacts.sniffing(),now,chrome);});
        chrome.title="REQUEST DETAIL";
        if(request_detail.active())deckSave("deck-request-detail",[&]{ui::drawContactDetailView(request_detail,contacts.sniffing(),now,chrome);});
        if(client_detail.active()){
            model::ApRow named;named.bssid=client_detail.key().target;named.ssid="LONG CACHED OBSERVATORY NAME 1234";
            model::ContactInspection named_detail;named_detail.open(contacts,false,0,&named);
            save("client-named-detail",[&](auto &d,int t){ui::drawExternalContactDetailStrip(d,t,contacts,named_detail,now);});
            chrome.title="CLIENT DETAIL";deckSave("deck-client-named",[&]{ui::drawContactDetailView(named_detail,contacts.sniffing(),now,chrome);});
        }
        if(!empty){
            auto crowded=contacts;std::vector<std::string> crowd_rows;
            for(unsigned i=0;i<30;++i){char row[160];snprintf(row,sizeof row,"02:00:00:00:00:aa,02:00:00:00:01:%02x,6,2.4,%d,%u",i,-40-int(i%5)*9,i*17);crowd_rows.emplace_back(row);}
            crowded.absorbClients(frame(OCP_MARK_CLIENTS,crowd_rows),now-500);
            save("contacts-crowded",[&](auto &d,int t){ui::drawExternalContactsStrip(d,t,crowded,false,29,now);});
            std::vector<std::string> ap_rows;
            for(unsigned i=0;i<7;++i){char row[160];snprintf(row,sizeof row,"02:00:00:00:00:%02x,02:00:00:00:01:ee,6,2.4,-54,%u",i,17+i);ap_rows.emplace_back(row);}
            crowded.absorbClients(frame(OCP_MARK_CLIENTS,ap_rows),now-500);
            model::ContactInspection multi;multi.open(crowded,false,6);
            save("client-multiple-links",[&](auto &d,int t){ui::drawExternalContactDetailStrip(d,t,crowded,multi,now);});
        }
        if(client_detail.active()){
            auto missing=contacts;missing.absorbClients(frame(OCP_MARK_CLIENTS,{}),now);
            client_detail.refresh(missing);
            chrome.title="CLIENT DETAIL";deckSave("deck-client-missing",[&]{ui::drawContactDetailView(client_detail,missing.sniffing(),now,chrome);});
            save("client-missing",[&](auto &d,int t){ui::drawExternalContactDetailStrip(d,t,missing,client_detail,now);});
        }
        save("lora",[&](auto &d,int t){ui::drawExternalLoraStrip(d,t,lora,0,now);});
        {
            model::LoraSweepModel sw;
            if (!empty) {
                sw.begin(902000, 400, 64);
                for (int s = 0; s < 18; ++s) {
                    std::string csv;
                    for (int b = 0; b < 64; ++b) {
                        int v = -114 + ((b * 7 + s * 3) % 5);
                        if (b == 21) v = (s % 3 == 0) ? -58 : -76;
                        if (b == 22) v = -82;
                        if (b == 44 && s > 10) v = -68;
                        if (b) csv += ',';
                        csv += std::to_string(v);
                    }
                    ocp::Item ev;
                    ev.kind = ocp::ItemKind::Event;
                    ev.kv = {{OCP_K_KIND, OCP_EVT_KIND_LORA_SWEEP},
                             {OCP_K_START_KHZ, "902000"}, {OCP_K_STEP_KHZ, "400"},
                             {OCP_K_BINS, "64"}, {OCP_K_RSSI, csv}};
                    sw.absorbEvent(ev, now - static_cast<uint32_t>((18 - s) * 200));
                }
                if (cached) sw.stop();
            }
            save("lora-sweep", [&](auto &d, int t) { ui::drawExternalLoraSweepStrip(d, t, sw, now); });
        }
        save("pans",[&](auto &d,int t){ui::drawExternalZigStrip(d,t,zig,true,0);});
        save("nodes",[&](auto &d,int t){ui::drawExternalZigStrip(d,t,zig,false,0);});
        save("drive",[&](auto &d,int t){ui::drawExternalDriveStrip(d,t,gps,trail,log,now);});
        save("gnss",[&](auto &d,int t){ui::drawExternalGnssStrip(d,t,gps,now);});
        save("deauth",[&](auto &d,int t){ui::drawExternalDeauthStrip(d,t,deauth,0,false,now);});
        save("correlation",[&](auto &d,int t){ui::drawExternalAntiStrip(d,t,anti,0,false,now);});
        model::SystemStatus system;system.link=empty?ocp::LinkState::Disconnected:ocp::LinkState::Ready;
        system.probe_valid=!empty;system.deck_total=320*1024;system.sd_ready=true;system.deck_heap=180000;system.deck_min=150000;system.probe_heap=120000;system.probe_total=200000;
        model::SystemResources resources;
        system.probe_owner=OCP_OWNER_WIFI;system.probe_lora=OCP_K_RX;
        system.phy_task="CHANNEL 6";system.lora_logging=!empty;
        for(unsigned i=0;i<61;++i){
            const uint32_t stamp=now-60000+i*1000;
            system.deck_heap=175000+int(10000*std::sin(i*.18));
            if(i%2==0 && !(i>22&&i<32) && !(cached&&i>45)){
                system.probe_heap=110000+int(12000*std::sin(i*.23));
                system.probe_sample_ms=stamp;system.probe_uptime=i*1000;
                ++system.probe_sequence;
            }
            resources.observe(system,stamp);
        }
        save("link",[&](auto &d,int t){ui::drawExternalSystemStrip(d,t,system,now);});
        system.info=true;save("health",[&](auto &d,int t){ui::drawExternalSystemStrip(d,t,system,now,&resources);});
        const model::ApRow *ap=scan.rows().empty()?nullptr:&scan.rows()[5];model::Inspect inspect;inspect.valid=!empty;inspect.rssi=-54;inspect.mfp_capable=true;inspect.rsn=true;
        model::WifiInspection ap_detail;model::ContactsModel wifi_contacts;
        if(ap){ap_detail.open(scan,5);inspect.bssid=ap->bssid;inspect.beacons=23;inspect.interval_ms=100;ap_detail.accept(inspect,now-300);
            wifi_contacts.begin();wifi_contacts.absorbClients(frame(OCP_MARK_CLIENTS,{ap->bssid+",02:00:00:00:01:01,6,2.4,-61,123",ap->bssid+",02:00:00:00:01:02,6,2.4,-74,42"}),now-12000);
            wifi_contacts.stopSniffing();
        }
        save("ap-detail",[&](auto &d,int t){ui::drawExternalWifiDetailStrip(d,t,ap_detail,scan,wifi_contacts,false,now);});
        chrome.title="AP DOSSIER";
        deckSave("deck-ap-radio",[&]{ui::drawWifiDetailView(ap_detail,scan,wifi_contacts,false,now,chrome);});
        ap_detail.togglePage();ap_detail.movePeer(wifi_contacts,1);
        save("ap-clients",[&](auto &d,int t){ui::drawExternalWifiDetailStrip(d,t,ap_detail,scan,wifi_contacts,false,now);});
        deckSave("deck-ap-clients",[&]{ui::drawWifiDetailView(ap_detail,scan,wifi_contacts,false,now,chrome);});
        model::BtInspection advertiser;advertiser.open(bt,5);
        save("advertiser-detail",[&](auto &d,int t){ui::drawExternalBtDetailStrip(d,t,advertiser,bt,now);});
        chrome.title="BLE DOSSIER";deckSave("deck-advertiser",[&]{ui::drawBtDetailView(advertiser,bt,now,chrome);});
        if(advertiser.active()){
            advertiser.close();
            for(unsigned i=0;i<12;++i){auto measured=frame(OCP_MARK_BLE,{advertiser.row().mac+",FIELD SENSOR 06,004c,,-"+std::to_string(45+i%5*5)+",5"});
                bt.absorbScan(measured,now-110000+i*10000);if(!advertiser.active())advertiser.open(bt,0);else advertiser.refresh(bt);
            }
            bt.stop();
            save("advertiser-history",[&](auto &d,int t){ui::drawExternalBtDetailStrip(d,t,advertiser,bt,now);});
        }
        ui::SettingsViewState cfg;
        cfg.brightness = 146;
        cfg.idle_timeout_s = 60;
        cfg.idle_label = "60s";
        cfg.sd_ready = !empty;
        cfg.region_ok = !empty;
        cfg.region_status = empty ? "OFFLINE" : "SYNCED";
        cfg.band_ok = !empty;
        cfg.lora_from_sd = !empty;
        cfg.lora_label = empty ? "MCORE" : "EU868 MESH";
        cfg.lora_freq_hz = empty ? 910525000 : 869525000;
        cfg.lora_sf = empty ? 7 : 11;
        cfg.lora_bw_khz = empty ? 62 : 250;
        cfg.lora_sync = empty ? 0x12 : 0x2B;
        cfg.lora_count = empty ? 2 : 4;
        cfg.lora_index = empty ? 0 : 2;
        cfg.cursor = 0;
        save("settings",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
        chrome.title = "SETTINGS";
        deckSave("deck-settings", [&]{ ui::drawSettingsView(cfg, chrome); });
        if (!empty && !cached) {
            cfg.cursor = 1; save("settings-idle",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
            cfg.cursor = 2; save("settings-region",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
            cfg.cursor = 3; save("settings-band",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
            cfg.cursor = 4; save("settings-lora",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
            cfg.cursor = 5; cfg.audio_enabled = true;
            save("settings-audio",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
            cfg.cursor = 6; cfg.debug_enabled = true;
            save("settings-debug",[&](auto &d,int t){ui::drawExternalSettingsStrip(d,t,cfg);});
            deckSave("deck-settings-bottom", [&]{ ui::drawSettingsView(cfg, chrome); });
        }
        {
            model::SignalHunt h;
            h.startWifi("02:aa:bb:cc:dd:01", "Lab-Field-AP", 6, !empty);
            if (!empty) {
                h.setStreaming(!cached);
                const int samples[] = {-84,-82,-79,-76,-72,-68,-65,-61,-58,-55,-52,-49,-46,-44,-47,-51,-48,-45};
                for (size_t i = 0; i < sizeof(samples)/sizeof(samples[0]); ++i) {
                    h.observeSample(samples[i], now - static_cast<uint32_t>((18 - i) * 150));
                }
            }
            save("signal-hunt", [&](auto &d, int t) { ui::drawExternalHuntStrip(d, t, h, now); });
            if (!empty && !cached) {
                chrome.title = "SIGNAL HUNT";
                deckSave("deck-signal-hunt", [&]{ ui::drawSignalHuntView(h, now, chrome); });
            }
        }
    }
}
