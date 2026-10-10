#include "ui/external_drive_view.h"
#include "ui/theme.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
namespace ui {
namespace {
constexpr uint16_t ground=rgb565(0x060806),panel=rgb565(0x080C08),border=rgb565(0x1D2A1F);
}
void drawExternalDriveStrip(lgfx::LGFX_Sprite &d,int top,const model::GnssModel &gps,
                            const model::DriveTrail &trail,const storage::WardriveLogStats &log,uint32_t now)
{
    auto text=[&](int x,int y,uint16_t c,const char *s) {d.setTextSize(1);d.setTextColor(c,panel);d.setCursor(x,y-top);d.print(s);};
    auto box=[&](int x,int y,int w,int h) {d.fillRect(x,y-top,w,h,panel);d.drawRect(x,y-top,w,h,border);};
    auto line=[&](int x,int y,int xx,int yy,uint16_t c) {d.drawLine(x,y-top,xx,yy-top,c);};
    const bool fresh=gps.state(now)==model::GnssState::Fixed && gps.fixAgeMs(now)<3000;
    char value[64];d.fillScreen(ground);box(4,3,312,29);
    const char *labels[]={"LOG TIME","RAM DIST","FIX / HDOP","SAVED AP"};
    for(int tile=0;tile<4;++tile) {
        const int x=8+tile*78;if(tile) line(x-4,6,x-4,28,border);
        text(x,6,kMutedSlate,labels[tile]);
        if(tile==0) {const auto secs=trail.duration(now)/1000;snprintf(value,sizeof value,"%02lu:%02lu:%02lu",(unsigned long)(secs/3600),(unsigned long)(secs/60%60),(unsigned long)(secs%60));}
        else if(tile==1) {if(trail.size()) snprintf(value,sizeof value,"%.0fm",trail.distance());else snprintf(value,sizeof value,"--");}
        else if(tile==2) {if(fresh) snprintf(value,sizeof value,"FIX / %.1f",(double)gps.fix().hdop);else snprintf(value,sizeof value,"%s",gps.state(now)==model::GnssState::Searching ? "SEARCH" : "NO FIX");}
        else snprintf(value,sizeof value,"%lu",(unsigned long)log.aps);
        text(x,18,tile==0 ? kCalibrationYellow : tile==3 ? kSignalPink : kPaperPhosphor,value);
    }
    box(4,35,312,140);text(8,39,kMutedSlate,trail.cached() ? "CACHED TRACK / SESSION CLOSED" : "RAM BREADCRUMB / NORTH UP");
    for(int x=28;x<312;x+=40) line(x,52,x,170,border);
    for(int y=58;y<171;y+=28) line(8,y,310,y,border);
    text(294,56,kFieldGreen,"N");line(296,80,296,68,kFieldGreen);line(296,68,293,73,kFieldGreen);line(296,68,299,73,kFieldGreen);
    if(!trail.size()) {
        text(40,96,kCalibrationYellow,"WAITING FOR FRESH GPS FIX");
        text(40,110,kMutedSlate,"NO ROUTE POINTS RECORDED");
    } else {
        double east[model::DriveTrail::kCapacity],north[model::DriveTrail::kCapacity];
        double minx=0,maxx=0,miny=0,maxy=0;
        const auto &origin=trail.point(trail.size()-1);
        for(size_t i=0;i<trail.size();++i) {
            const auto &p=trail.point(i);double lon=p.lon-origin.lon;
            if(lon>180) lon-=360;
            if(lon< -180) lon+=360;
            east[i]=lon*111320*std::cos(origin.lat*0.0174532925199433);north[i]=(p.lat-origin.lat)*111320;
            minx=std::min(minx,east[i]);maxx=std::max(maxx,east[i]);miny=std::min(miny,north[i]);maxy=std::max(maxy,north[i]);
        }
        const double scale=std::max(std::max((maxx-minx)/250,(maxy-miny)/95),0.5);
        int lastx=0,lasty=0;
        for(size_t i=0;i<trail.size();++i) {
            const int x=160+(east[i]-(minx+maxx)/2)/scale;
            const int y=112-(north[i]-(miny+maxy)/2)/scale;
            if(i && !trail.point(i).gap) line(lastx,lasty,x,y,kFieldGreen);
            d.fillCircle(x,y-top,i+1==trail.size() ? 3 : 1,i+1==trail.size() ? kCalibrationYellow : kDimGreen);
            lastx=x;lasty=y;
        }
        snprintf(value,sizeof value,"SPAN %.0fm / %u POINTS",scale*250,(unsigned)trail.size());text(8,163,kMutedSlate,value);
    }
    box(4,178,312,35);
    if(fresh) snprintf(value,sizeof value,"%.5f / %.5f",gps.fix().lat_deg,gps.fix().lon_deg);
    else snprintf(value,sizeof value,"GNSS %s / %s",gps.state(now)==model::GnssState::Fixed ? "aged fix" : model::gnssStateName(gps.state(now)),gps.hasUartData(now) ? "UART RECEIVING" : "UART SILENT");
    text(8,183,fresh ? kPaperPhosphor : kCalibrationYellow,value);
    snprintf(value,sizeof value,"SD TRACK %lu / NO-FIX AP %lu",(unsigned long)log.track_points,(unsigned long)log.aps_no_fix);text(8,197,kMutedSlate,value);
    box(4,217,224,20);text(8,220,log.open ? kFieldGreen : kMutedSlate,log.open ? "SESSION OPEN / FIX REQUIRED" : "LOG IDLE / RAM VIEW");
    text(8,229,kMutedSlate,"PASSIVE / CONTROL ON DECK");
}
}
