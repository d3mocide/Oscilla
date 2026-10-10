/* Bounded RAM breadcrumb; fresh fixes only, gaps never joined. */
#pragma once
#include <array>
#include <cmath>
#include "model/gnss_model.h"
namespace model {
struct DrivePoint { double lat=0, lon=0; bool gap=true; };
class DriveTrail {
public:
    static constexpr size_t kCapacity=64;
    void observe(const GnssFix &fix, bool fresh, uint32_t fix_ms, uint32_t now, bool logging) {
        if(logging && !logging_) { size_=0; distance_=0; gap_=true; seen_fix_=false; started_=now; seen_session_=true; }
        if(!logging && logging_) duration_=now-started_;
        logging_=logging;
        if(!fresh || !fix.valid || !std::isfinite(fix.lat_deg) || !std::isfinite(fix.lon_deg) ||
           std::abs(fix.lat_deg)>90 || std::abs(fix.lon_deg)>180) {gap_=true; return;}
        if((seen_session_ && !logging) || (seen_fix_ && fix_ms==last_fix_)) return;
        if(size_ && !gap_) {
            const auto &last=points_[size_-1];
            double lon=fix.lon_deg-last.lon;
            if(lon>180) lon-=360;
            if(lon< -180) lon+=360;
            const double x=lon*111320*std::cos((fix.lat_deg+last.lat)*0.5*0.0174532925199433);
            const double y=(fix.lat_deg-last.lat)*111320;
            distance_+=std::hypot(x,y);
        }
        if(size_==kCapacity) {
            for(size_t i=1;i<size_;++i) points_[i-1]=points_[i];
            --size_; points_[0].gap=true;
        }
        points_[size_++]={fix.lat_deg,fix.lon_deg,gap_};
        gap_=false; seen_fix_=true; last_fix_=fix_ms;
    }
    size_t size() const {return size_;}
    const DrivePoint &point(size_t i) const {return points_[i];}
    double distance() const {return distance_;}
    bool logging() const {return logging_;}
    bool cached() const {return seen_session_ && !logging_;}
    uint32_t duration(uint32_t now) const {return logging_ ? now-started_ : duration_;}
private:
    std::array<DrivePoint,kCapacity> points_{};
    size_t size_=0;
    bool gap_=true, seen_fix_=false, logging_=false, seen_session_=false;
    uint32_t last_fix_=0, started_=0, duration_=0;
    double distance_=0;
};
}
