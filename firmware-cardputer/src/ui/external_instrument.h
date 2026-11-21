/* Shared graphical vocabulary for the 320x240 companion; brand README. */
#pragma once
#include <M5GFX.h>
#include <algorithm>
#include <string>
#include "ui/theme.h"

namespace ui { namespace instrument {
inline std::string safe(const std::string &s, size_t limit)
{
    std::string out;
    for (unsigned char c : s) {
        if (out.size() >= limit) break;
        out += c >= 32 && c <= 126 ? char(c) : '.';
    }
    return out;
}

class Canvas {
    lgfx::LGFX_Sprite &d_;
    int top_;
public:
    Canvas(lgfx::LGFX_Sprite &d, int top) : d_(d), top_(top) {}
    int top() const { return top_; }
    int stripHeight() const { return d_.height(); }
    void pixel(int x,int y,uint16_t c) {
        const int sy=y-top_;
        if(x>=0&&x<d_.width()&&sy>=0&&sy<d_.height()) d_.drawPixel(x,sy,c);
    }
    void clear() { d_.fillScreen(kVoidInk); }
    void line(int x,int y,int xx,int yy,uint16_t c=kLineBorder) {
        d_.drawLine(x,y-top_,xx,yy-top_,c);
    }
    void box(int x,int y,int w,int h,uint16_t c) { d_.drawRect(x,y-top_,w,h,c); }
    void rect(int x,int y,int w,int h,uint16_t c) { d_.fillRect(x,y-top_,w,h,c); }
    void dot(int x,int y,int r,uint16_t c) { d_.fillCircle(x,y-top_,r,c); }
    void ring(int x,int y,int r,uint16_t c) { d_.drawCircle(x,y-top_,r,c); }
    void text(int x,int y,const std::string &s,uint16_t c=kPaperPhosphor,int size=1,uint16_t background=kVoidInk) {
        if (x < 0 || x >= 320) return;
        const size_t limit=(320-x)/(6*size);
        auto label = safe(s, limit);
        if(s.size()>limit && limit>=3)label.replace(limit-3,3,"...");
        d_.setTextSize(size); d_.setTextColor(c,background);
        d_.setCursor(x,y-top_); d_.print(label.c_str());
    }
    void corners(int x,int y,int w,int h,uint16_t c=kDimGreen) {
        for (int side : {0,1}) for (int end : {0,1}) {
            const int xx=x+side*(w-1), yy=y+end*(h-1);
            line(xx,yy,xx+(side?-6:6),yy,c); line(xx,yy,xx,yy+(end?-6:6),c);
        }
    }
    void field(const char *label,const char *state,uint16_t colour=kMutedSlate) {
        clear(); text(12,10,label,kMutedSlate);
        text(320-12-int(std::char_traits<char>::length(state))*6,10,state,colour);
        corners(8,29,304,119); line(12,155,308,155,kDimGreen);
    }
    void grid(int x,int y,int w,int h) {
        for(int xx=x;xx<=x+w;xx+=40) line(xx,y,xx,y+h,kTrackDark);
        for(int yy=y;yy<=y+h;yy+=28) line(x,yy,x+w,yy,kTrackDark);
    }
    void meter(int x,int y,int w,int h,int percent,uint16_t c=kFieldGreen,int segments=16) {
        const int active=std::max(0,std::min(segments,(percent*segments+99)/100));
        const int step=w/segments;
        for(int i=0;i<segments;++i) rect(x+i*step,y,step-3,h,i<active?c:kTrackDark);
    }
    void signal(int x,int y,int w,int h,int rssi,uint16_t c=kFieldGreen) {
        meter(x,y,w,h,std::max(0,std::min(100,(rssi+100)*100/70)),c);
    }
    void metric(int x,int y,const char *label,const std::string &value,uint16_t c=kPaperPhosphor,int size=2,int width=132) {
        while(size>1 && value.size()*6*size>size_t(width))--size;
        text(x,y,label,kMutedSlate); text(x,y+14,value,c,size);
    }
};

inline size_t firstVisible(size_t size,size_t cursor,size_t capacity=12)
{
    if (size<=capacity) return 0;
    return std::min(cursor>capacity/2 ? cursor-capacity/2 : 0,size-capacity);
}

inline void signalColumn(Canvas &c,int x,int width,int rssi,bool selected,const std::string &label)
{
    const int h=std::max(2,std::min(84,(rssi+100)*84/70));
    const uint16_t colour=selected?kCalibrationYellow:kFieldGreen;
    c.rect(x,132-h,width,h,selected?kDimGreen:kTrackDark);
    c.line(x,132-h,x+width-1,132-h,colour);
    for(int y=132-h+5;y<132;y+=7)c.line(x+1,y,x+width-2,y,selected?kFieldGreen:kDimGreen);
    if(selected)c.corners(x-2,129-h,width+4,h+7,kSignalPink);
    c.text(x,138,label,selected?kCalibrationYellow:kMutedSlate);
}
} }
