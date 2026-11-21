/* Frozen raw payload shared with the companion inspection page. */
#include "ui/lora_detail_view.h"
#include "ui/canvas.h"
#include "ui/theme.h"
namespace ui {
void drawLoraDetailView(const model::PacketInspection &p,const ChromeState &chrome,const char *parent)
{
    beginChrome(chrome);auto &d=canvas();const auto &packet=p.packet();
    d.setTextSize(1);d.setTextColor(kCalibrationYellow,kVoidInk);
    d.setCursor(5,kBodyTop+2);d.printf("%uB %ddBm %+.1f / RAW",packet.len,packet.rssi,(double)packet.snr);
    for(size_t row=0;row<8;++row){
        const auto line=p.line(row);if(!line.bytes)break;
        d.setCursor(5,kBodyTop+15+row*9);d.setTextColor(kMutedSlate,kVoidInk);
        d.printf("%02X ",(unsigned)(p.offset()+row*8));
        d.setTextColor(kPaperPhosphor,kVoidInk);d.print(line.hex.c_str());
        d.setCursor(169,kBodyTop+15+row*9);d.setTextColor(kFieldGreen,kVoidInk);d.print(line.ascii.c_str());
    }
    d.setCursor(5,kBodyTop+91);d.setTextColor(kCalibrationYellow,kVoidInk);
    d.printf("; . PAGE %u/%u / ` %s",(unsigned)(p.offset()/64+1),(unsigned)std::max(1,(packet.len+63)/64),parent);
    endChrome(chrome);
}
}
