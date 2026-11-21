/* Source-render QA canvas; includes the installed M5GFX font without copying it. */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
#define PROGMEM
#include <lgfx/Fonts/glcdfont.h>
constexpr uint16_t TFT_BLACK=0,TFT_WHITE=0xffff,TFT_GREEN=0x07e0,TFT_YELLOW=0xffe0;
namespace lgfx {
class LGFX_Sprite {
    int w_=320,h_=24,x_=0,y_=0,size_=1;
    uint16_t colour_=0xffff,bg_=0;
    std::vector<uint16_t> pixels_;
public:
    LGFX_Sprite():pixels_(320*24) {}
    bool createSprite(int w,int h) {w_=w;h_=h;pixels_.assign(w*h,0);return true;}
    void setColorDepth(int) {}
    const void *getBuffer()const{return pixels_.data();}
    int width()const{return w_;}int height()const{return h_;}
    uint16_t readPixel(int x,int y)const{return x<0||y<0||x>=w_||y>=h_?0:pixels_[y*w_+x];}
    void drawPixel(int x,int y,uint16_t c){if(x>=0&&y>=0&&x<w_&&y<h_)pixels_[y*w_+x]=c;}
    void fillScreen(uint16_t c){std::fill(pixels_.begin(),pixels_.end(),c);}
    void fillRect(int x,int y,int w,int h,uint16_t c){for(int yy=std::max(0,y);yy<std::min(h_,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(w_,x+w);++xx)drawPixel(xx,yy,c);}
    void drawLine(int x,int y,int xx,int yy,uint16_t c){int dx=std::abs(xx-x),sx=x<xx?1:-1,dy=-std::abs(yy-y),sy=y<yy?1:-1,e=dx+dy;for(;;){drawPixel(x,y,c);if(x==xx&&y==yy)break;int e2=2*e;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
    void drawFastHLine(int x,int y,int w,uint16_t c){drawLine(x,y,x+w-1,y,c);}
    void drawRect(int x,int y,int w,int h,uint16_t c){drawLine(x,y,x+w-1,y,c);drawLine(x,y+h-1,x+w-1,y+h-1,c);drawLine(x,y,x,y+h-1,c);drawLine(x+w-1,y,x+w-1,y+h-1,c);}
    void drawCircle(int x,int y,int r,uint16_t c){for(int i=0;i<360;++i){double a=i*3.141592653589793/180;drawPixel(x+std::lround(r*std::cos(a)),y+std::lround(r*std::sin(a)),c);}}
    void fillCircle(int x,int y,int r,uint16_t c){for(int yy=-r;yy<=r;++yy)for(int xx=-r;xx<=r;++xx)if(xx*xx+yy*yy<=r*r)drawPixel(x+xx,y+yy,c);}
    void setTextSize(int n){size_=n;}void setTextColor(uint16_t c,uint16_t b=0){colour_=c;bg_=b;}
    void setCursor(int x,int y){x_=x;y_=y;}int textWidth(const char*s)const{return std::char_traits<char>::length(s)*6*size_;}
    void print(const char*s){for(unsigned char c:std::string(s)){if(c=='\n'){y_+=8*size_;x_=0;continue;}fillRect(x_,y_,6*size_,8*size_,bg_);for(int col=0;col<5;++col)for(int row=0;row<8;++row)if(font[c*5+col]&(1<<row))fillRect(x_+col*size_,y_+row*size_,size_,size_,colour_);x_+=6*size_;}}
    void println(const char*s=""){print(s);y_+=8*size_;x_=0;}
    void printf(const char*fmt,...){char b[256];va_list a;va_start(a,fmt);vsnprintf(b,sizeof b,fmt,a);va_end(a);print(b);}
};
}
