#include "model/session_review.h"
#include "storage/lora_log_format.h"
#include "storage/wardrive_csv.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace model;
    SessionEntry e;
    assert(sessionName("drive_0001.csv", SessionKind::Drive, e));
    assert(e.index == 1 && e.parts == Csv);
    assert(!sessionName("../drive_0001.csv", SessionKind::Drive, e));
    assert(!sessionName("drive_0000.csv", SessionKind::Drive, e));
    assert(sessionName("log_9999.health.csv", SessionKind::Lora, e) && e.parts == Health);
    SessionCatalog c;
    c.add(e);
    assert(sessionName("log_9999.csv", SessionKind::Lora, e));
    c.add(e);
    assert(c.entries.size() == 1 && c.entries[0].parts == (Csv | Health));
    for (int i = 1; i < 100; ++i) {
        e = {SessionKind::Drive, i, Csv, 30};
        c.add(e);
    }
    assert(c.entries.size() == 64 && c.overflow);
    for(int cr=1;cr<=4;++cr){
        model::LoraPacket packet;packet.len=2;packet.hex="0041";packet.rssi=-180;packet.snr=126.5;
        model::SessionParser roundtrip;roundtrip.begin(SessionKind::Lora);
        for(char ch:storage::loraLogHeader()+storage::loraLogRow(12345,910525000,7,62,cr,packet))roundtrip.feed(ch);
        roundtrip.finish();assert(roundtrip.summary.valid==1&&roundtrip.summary.bad==0&&roundtrip.page[0].cr==cr);
    }
    {
        model::ApRow ap;ap.bssid="02:00:00:00:00:01";ap.ssid="test,\"\n";ap.auth="WPA2";ap.ch=6;ap.rssi=-128;
        model::SessionParser roundtrip;roundtrip.begin(SessionKind::Drive);
        auto saved=storage::wardriveCsvHeader("test")+storage::wardriveCsvColumnHeader()+storage::wardriveCsvRow(ap,2026,11,17,12,30,0,45.0,-123.0,5.0,4.0);
        for(char ch:saved)roundtrip.feed(ch);
        roundtrip.finish();assert(roundtrip.summary.valid==1&&roundtrip.summary.bad==0&&roundtrip.page[0].name==ap.ssid);
    }
    SessionParser p;
    p.begin(SessionKind::Lora);
    std::string header = "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing\n";
    std::string rows = "1000,910525000,7,62,1,-61,4.5,2,00FF,unknown\n2000,910525000,7,62,1,-65,2.5,2,12,"
                       "unknown\n3000,910525000,7,62,1,-70,1.5,1,41,unknown\n";
    for (char ch : header + rows) p.feed(ch);
    p.finish();
    assert(p.summary.valid == 2 && p.summary.bad == 1 && p.summary.spanMs() == 2000 && p.page.size() == 2 &&
           p.page[0].packet.hex == "00FF");
    p.begin(SessionKind::Lora);
    for (char ch : header + std::string(900, 'X') + "\n" + rows) p.feed(ch);
    p.finish();
    assert(p.summary.bad == 2 && p.summary.valid == 2);
    p.begin(SessionKind::Lora);
    for (char ch : header + rows + "4000,broken") p.feed(ch);
    p.finish();
    assert(p.summary.partial && p.summary.valid == 2);
    p.begin(SessionKind::Lora);
    for (char ch : std::string("wrong\n") + rows) p.feed(ch);
    p.finish();
    assert(!p.summary.header && p.summary.valid == 0);
    p.begin(SessionKind::Drive);
    std::string drive = "WigleWifi-1.6,appRelease=test\nMAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,"
                        "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,"
                        "Type\n02:00:00:00:00:01,test\\012name,[ESS],2026-11-17 "
                        "12:00:00,6,2437,-60,45.1,-123.2,5.0,4.0,,,WIFI\n02:00:00:00:00:02,other,[ESS],2026-"
                        "11-17 12:01:00,1,2412,-70,45.2,-123.3,5.0,4.0,,,WIFI\n";
    for (char ch : drive) p.feed(ch);
    p.finish();
    assert(p.summary.valid == 2 && p.summary.spanMs() == 60000 && p.page[0].name == "test\nname");
    p.begin(SessionKind::Lora, 1);
    for (char ch : header) p.feed(ch);
    for (int i = 0; i < 15; ++i)
        for (char ch : std::to_string(1000 + i) + ",910525000,7,62,1,-61,4.5,1,41,unknown\n") p.feed(ch);
    p.finish();
    assert(p.summary.valid == 15 && p.page.size() == 6 && p.page[0].packet.received_ms == 1006);
    SessionReview review;
    review.catalog = c;
    review.stage = ReviewStage::Summary;
    review.packet.open(p.page[0].packet);
    review.stage = ReviewStage::Hex;
    review.back();
    assert(review.stage == ReviewStage::Records && !review.packet.active());
    review.back();
    assert(review.stage == ReviewStage::Summary);
    review.back();
    assert(review.stage == ReviewStage::Catalog);
    std::cout << "session review: strict names, grouped bounded catalog, hostile rows, chunking, partial "
                 "tails, paging and Back passed\n";
}
