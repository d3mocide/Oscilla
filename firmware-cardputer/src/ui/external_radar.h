/* Strength rings; identity determines angle, never geographic bearing. */
#pragma once
#include "ui/external_instrument.h"
#include "ui/observatory_chrome.h"
#include <cmath>
namespace ui { namespace instrument {
struct RadarPoint{int x,y;};
inline RadarPoint orbitPoint(const std::string &id,int radius,int cx=94,int cy=88){
    uint32_t hash=2166136261U;for(unsigned char c:id)hash=(hash^c)*16777619U;
    hash^=hash>>16;hash*=0x7feb352dU;hash^=hash>>15;hash*=0x846ca68bU;hash^=hash>>16;
    const double angle=(hash%360)*0.0174532925199433;
    return {cx+int(std::cos(angle)*radius),cy+int(std::sin(angle)*radius)};
}
inline RadarPoint radarPoint(const std::string &id,int rssi,int cx=94,int cy=88){
    return orbitPoint(id,std::max(10,std::min(51,10+(-rssi-30)*41/70)),cx,cy);
}
inline void radar(Canvas &c){
    for(int radius:{19,35,51})c.ring(94,88,radius,kDimGreen);
    c.line(37,88,151,88,kTrackDark);c.line(94,34,94,142,kTrackDark);
    c.corners(87,81,15,15,kPaperPhosphor);c.dot(94,88,2,kFieldGreen);
    c.text(158,42,"-40",kMutedSlate);c.text(158,65,"-65",kMutedSlate);c.text(158,88,"-95",kMutedSlate);
    c.text(198,132,"RSSI / dBm",kMutedSlate);
}
inline void radarNode(Canvas &c,RadarPoint p,bool selected,bool diamond=false){
    const auto color=selected?kCalibrationYellow:kFieldGreen;
    if(diamond){c.line(p.x,p.y-3,p.x+3,p.y,color);c.line(p.x+3,p.y,p.x,p.y+3,color);
        c.line(p.x,p.y+3,p.x-3,p.y,color);c.line(p.x-3,p.y,p.x,p.y-3,color);}
    else c.dot(p.x,p.y,2,color);
    if(selected){c.ring(p.x,p.y,6,kSignalPink);c.corners(p.x-9,p.y-9,19,19,kCalibrationYellow);}
}

/* Observatory v2 astrolabe reticle with truthfully anchored meridian dBm rings. */
inline void obsRadar(Canvas &c,int cx=84,int cy=88){
    using namespace observatory;
    obsHexagon(c,cx,cy,56,kTrackDark);
    obsDashedCircle(c,cx,cy,51,kDimGreen,36);
    c.ring(cx,cy,35,kDimGreen);
    c.ring(cx,cy,18,kTrackDark);
    c.line(cx-54,cy,cx+54,cy,kTrackDark);
    c.line(cx,cy-54,cx,cy+54,kTrackDark);
    for(int i=0;i<12;++i){
        const double a=i*0.5235987755982989;
        const int r0=(i%3==0)?47:49;
        c.line(cx+int(std::lround(std::cos(a)*r0)),cy+int(std::lround(std::sin(a)*r0)),
               cx+int(std::lround(std::cos(a)*53)),cy+int(std::lround(std::sin(a)*53)),
               (i%3==0)?kDimGreen:kTrackDark);
    }
    /* Anchor ring scales directly on their true inner-to-outer rings (strong=-40 inner). */
    const struct { int r; const char *lbl; } rings[]={{18,"-40"},{35,"-65"},{51,"-95"}};
    for(const auto &rg:rings){
        c.rect(cx-10,cy-rg.r-4,20,8,kVoidInk);
        obsMicro(c,cx,cy-rg.r-3,rg.lbl,kMutedSlate,kVoidInk,Align::Center);
    }
    obsDiamond(c,cx,cy,4,kPaperPhosphor);
    c.dot(cx,cy,1,kFieldGreen);
}
inline void obsRadarNode(Canvas &c,RadarPoint p,bool selected,bool diamond,bool cached){
    using namespace observatory;
    if(!selected){
        const uint16_t col=cached?kMutedSlate:kFieldGreen;
        if(diamond){obsDiamondFill(c,p.x,p.y,2,kVoidInk);obsDiamond(c,p.x,p.y,3,col);}
        else c.dot(p.x,p.y,2,col);
        return;
    }
    obsDashedCircle(c,p.x,p.y,8,kSignalPink,16);
    obsTargetLock(c,p.x,p.y,11,kCalibrationYellow);
    if(diamond)obsDiamondFill(c,p.x,p.y,4,kCalibrationYellow);
    else c.dot(p.x,p.y,3,kCalibrationYellow);
}
} }
