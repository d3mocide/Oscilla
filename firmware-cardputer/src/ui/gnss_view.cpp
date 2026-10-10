/* Rendered reference: master guide Drive / Session deck. */
#include "ui/gnss_view.h"
#include <M5Cardputer.h>
#include <cstdio>
#include "storage/wardrive_logger.h"
#include "ui/canvas.h"
#include "ui/theme.h"
namespace ui {
void drawGnssView(const model::GnssModel &gps,uint32_t now,const ChromeState &chrome)
{
    auto &d=canvas();beginChrome(chrome);d.setTextSize(1);
    const auto state=gps.state(now);const auto &fix=gps.fix();const auto &log=storage::wardriveLogStats();
    const bool fresh=state==model::GnssState::Fixed && gps.fixAgeMs(now)<3000;
    d.setCursor(5,kBodyTop+5);d.setTextColor(fresh ? kPaperPhosphor : kCalibrationYellow,kVoidInk);
    if(fresh) d.printf("%.4f / %.4f",fix.lat_deg,fix.lon_deg);
    else d.printf("GNSS %s",state==model::GnssState::Fixed ? "aged fix" : model::gnssStateName(state));
    d.drawFastHLine(5,kBodyTop+17,230,kDimGreen);
    const char *labels[]={"TARGETS SAVED","FIX QUALITY","TRACK / NOFIX","SD SESSION"};
    char value[4][40];
    snprintf(value[0],sizeof value[0],"%lu AP",(unsigned long)log.aps);
    if(fresh) snprintf(value[1],sizeof value[1],"%.1f HDOP",(double)fix.hdop);
    else snprintf(value[1],sizeof value[1],"%s",state==model::GnssState::FixLost ? "FIX LOST" : state==model::GnssState::Fixed ? "AGED FIX" : gps.hasUartData(now) ? "SEARCHING" : "UART SILENT");
    snprintf(value[2],sizeof value[2],"%lu / %lu",(unsigned long)log.track_points,(unsigned long)log.aps_no_fix);
    snprintf(value[3],sizeof value[3],"%s",log.open ? "RECORDING" : "CLOSED");
    for(int i=0;i<4;++i) {
        const int y=kBodyTop+25+i*15;const uint16_t bg=i==1 ? kSelectionGlow : kVoidInk;
        if(i==1) {d.fillRect(4,y-2,232,14,bg);d.drawRect(4,y-2,232,14,kCalibrationYellow);}
        d.setTextColor(i==1 ? kCalibrationYellow : kPaperPhosphor,bg);d.setCursor(7,y);d.print(labels[i]);
        d.setCursor(125,y);d.print(value[i]);
    }
    d.drawFastHLine(5,kBodyTop+84,230,kDimGreen);
    d.setCursor(5,kBodyTop+88);d.setTextColor(log.open ? kFieldGreen : kMutedSlate,kVoidInk);
    d.print(log.open ? "l CLOSE LOG / FRESH FIX REQUIRED" : "l START LOG / EXTERNAL TRACK");
    endChrome(chrome);
}
}
