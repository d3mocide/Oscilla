#include "model/device_inspection.h"
#include "ocp.h"
#include <cassert>
#include <iostream>
static ocp::Item table(const char *tag,std::vector<std::string> rows){ocp::Item x;x.kind=ocp::ItemKind::Frame;x.tag=tag;x.rows=rows;x.kv={{OCP_K_FIRST,"1"},{OCP_K_TOTAL,std::to_string(rows.size())}};return x;}
int main(){
 model::ScanModel scan;scan.begin();scan.absorbPage(table(OCP_MARK_SCAN,{"1,same,02:00:00:00:00:01,6,RSN,-61,2.4","2,same,02:00:00:00:00:02,1,OPEN,-70,2.4"}),1000);
 model::WifiInspection ap;assert(ap.open(scan,0));assert(ap.row().bssid=="02:00:00:00:00:01");
 scan.begin();scan.absorbPage(table(OCP_MARK_SCAN,{"1,same,02:00:00:00:00:02,1,OPEN,-75,2.4","2,changed,02:00:00:00:00:01,6,RSN,-55,2.4"}),2000);
 ap.refresh(scan);assert(ap.present()&&ap.row().idx==2&&ap.row().rssi==-55);
 model::Inspect in;in.valid=true;in.idx=2;in.bssid="02:00:00:00:00:02";in.beacons=5;in.rssi=-20;
 assert(!ap.accept(in,3000));assert(!ap.inspect().valid);
 in.bssid=ap.row().bssid;assert(ap.accept(in,3000));assert(ap.signal()==-20);
 scan.begin();ap.refresh(scan);assert(!ap.present()&&ap.row().ssid=="changed");
 model::BtModel bt;bt.beginScan();bt.absorbScan(table(OCP_MARK_BLE,{"02:00:00:00:00:01,name,004c,,-61,2"}),1000);
 model::BtInspection device;assert(device.open(bt,0));assert(device.history().size()==1);
 device.refresh(bt);device.refresh(bt);assert(device.history().size()==1);
 bt.beginScan();device.refresh(bt);assert(!device.present());
 bt.absorbScan(table(OCP_MARK_BLE,{"02:00:00:00:00:02,other,,, -30,1","02:00:00:00:00:01,name,004c,,-55,5"}),8000);
 device.refresh(bt);assert(device.present()&&device.row().rssi==-55&&device.history().size()==2);
 auto chart=model::signalPlot(device.history(),8000,true);assert(chart.size==2&&chart.points[1].connected);
 bt.beginScan();bt.absorbScan(table(OCP_MARK_BLE,{}),9000);device.refresh(bt);
 assert(!device.present()&&device.row().mac=="02:00:00:00:00:01"&&device.history().size()==2);
 model::ContactsModel peers;peers.begin();peers.absorbClients(table(OCP_MARK_CLIENTS,{"02:00:00:00:00:01,02:00:00:00:01:01,6,2.4,-61,5","02:00:00:00:00:01,02:00:00:00:01:02,6,2.4,-65,6","02:00:00:00:00:02,02:00:00:00:01:03,1,2.4,-70,7"}),4000);
 assert(ap.peerCount(peers)==2);ap.movePeer(peers,1);assert(ap.peer(peers)->mac=="02:00:00:00:01:02");
 ap.movePeer(peers,1);assert(ap.peer(peers)->mac=="02:00:00:00:01:02");ap.togglePage();assert(ap.clientsPage());
 bt.beginContinuous();ocp::Item malformed;malformed.kind=ocp::ItemKind::Event;malformed.kv={{OCP_K_KIND,OCP_EVT_KIND_BLE},{OCP_K_MAC,"02:00:00:00:00:01"}};
 bt.absorbEvent(malformed,10000);assert(bt.devices().empty());
 malformed.kv.push_back({OCP_K_RSSI,"-63"});bt.absorbEvent(malformed,11000);
 assert(bt.devices().size()==1&&bt.devices()[0].first_sighting&&bt.devices()[0].observed_ms==11000);
 device.refresh(bt);const auto count=device.history().size();device.refresh(bt);assert(device.history().size()==count);
 assert(std::string(device.stateLabel(bt,17000))=="AGED");
 model::SignalHistory history;for(unsigned i=0;i<40;++i)history.push(i*1000,-60-int(i%3));
 assert(history.size()==16);assert(model::signalPlot(history,200000,true).size==0);
 auto cached=model::signalPlot(history,200000,false);assert(cached.size==16);
 model::SignalHistory gaps;gaps.push(UINT32_MAX-1000,-60);gaps.push(1000,-50);gaps.push(20000,-45);
 chart=model::signalPlot(gaps,20001,true);assert(chart.size==3&&chart.points[1].connected&&!chart.points[2].connected);
 std::cout<<"device inspection: identities, reordered indices, foreign replies, real-only signal history, gaps and wrap passed\n";
}
