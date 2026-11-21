/* RAM-only relative-fix repeatability; no absolute position leaves this model. */
#pragma once
#include "model/gnss_model.h"
#include <cmath>
#include <algorithm>
namespace model {
class GnssNoise {
    bool active_=false,gap_=true;
    uint32_t samples_=0,last_ms_=0;
    std::string utc_;
    double lat_=0,lon_=0,x_=0,y_=0,path_=0,step_=0,mean_x_=0,mean_y_=0,m2_=0;
public:
    void start(){*this=GnssNoise{};active_=true;}
    void stop(){active_=false;}
    void observe(const GnssFix &f,bool fresh,uint32_t ms){
        if(!active_)return;
        if(!fresh||!f.valid||!std::isfinite(f.lat_deg)||!std::isfinite(f.lon_deg)||std::abs(f.lat_deg)>90||std::abs(f.lon_deg)>180){gap_=true;return;}
        if(samples_&&(ms==last_ms_||(!f.utc.empty()&&f.utc==utc_)))return;
        if(!samples_){lat_=f.lat_deg;lon_=f.lon_deg;}
        double lon=f.lon_deg-lon_;if(lon>180)lon-=360;if(lon< -180)lon+=360;
        const double x=lon*111320*std::cos(lat_*.0174532925199433),y=(f.lat_deg-lat_)*111320;
        if(samples_&&!gap_){const double distance=std::hypot(x-x_,y-y_);path_+=distance;step_=std::max(step_,distance);}
        ++samples_;const double dx=x-mean_x_,dy=y-mean_y_;
        mean_x_+=dx/samples_;mean_y_+=dy/samples_;m2_+=dx*(x-mean_x_)+dy*(y-mean_y_);
        x_=x;y_=y;last_ms_=ms;utc_=f.utc;gap_=false;
    }
    bool active()const{return active_;}
    uint32_t samples()const{return samples_;}
    double x()const{return x_;}double y()const{return y_;}
    double path()const{return path_;}double maxStep()const{return step_;}
    double rms()const{return samples_?std::sqrt(std::max(0.0,m2_/samples_)):0;}
};
}
