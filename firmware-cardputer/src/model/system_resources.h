/* Bounded, measured heap history; probe replies are sampled only once. */
#pragma once
#include <array>
#include <algorithm>
#include "model/system_status.h"
namespace model {
struct HeapSample {uint32_t ms=0,free=0,total=0;bool restart=false;};
class HeapHistory {
    std::array<HeapSample,61> samples_{};
    size_t next_=0,count_=0;
public:
    void push(HeapSample s){samples_[next_]=s;next_=(next_+1)%samples_.size();count_=std::min(count_+1,samples_.size());}
    size_t size()const{return count_;}
    const HeapSample &at(size_t i)const{return samples_[(next_+samples_.size()-count_+i)%samples_.size()];}
};
struct HeapPoint {uint32_t age=0;unsigned percent=0,basis_points=0;bool connected=false;};
struct HeapPlot {std::array<HeapPoint,61> points{};size_t size=0;};
template<class Visitor>
inline void visitHeapPoints(const HeapHistory &h,uint32_t now,uint32_t max_gap,Visitor visit){
    uint32_t previous=0;bool valid_previous=false;
    for(size_t i=0;i<h.size();++i){
        const auto &s=h.at(i);const uint32_t age=now-s.ms;
        if(age>60000||!s.total){valid_previous=false;continue;}
        HeapPoint p;p.age=age;
        p.basis_points=unsigned(std::min(uint64_t(10000),uint64_t(s.free)*10000/s.total));
        p.percent=p.basis_points/100;
        p.connected=valid_previous&&!s.restart&&s.ms-previous<=max_gap;
        visit(p);previous=s.ms;valid_previous=true;
    }
}
inline HeapPlot heapPlot(const HeapHistory &h,uint32_t now,uint32_t max_gap){
    HeapPlot out;visitHeapPoints(h,now,max_gap,[&](const HeapPoint &p){out.points[out.size++]=p;});return out;
}
struct HeapScale {
    unsigned low=0,high=10000;
    int y(unsigned value)const{
        value=std::max(low,std::min(high,value));
        return 131-(value-low)*78/(high-low);
    }
};
inline HeapScale heapScaleBounds(unsigned low,unsigned high,bool any){
    if(!any)return {};
    low=low>100?low-100:0;high=std::min(10000U,high+100);
    const unsigned span=std::max(1000U,high-low),center=(low+high)/2;
    low=center>span/2?center-span/2:0;high=low+span;
    if(high>10000){high=10000;low=high-span;}
    return {(low/500)*500,std::min(10000U,((high+499)/500)*500)};
}
inline HeapScale heapScale(const HeapPlot &deck,const HeapPlot &probe){
    unsigned low=10000,high=0;bool any=false;
    for(const auto *plot:{&deck,&probe})for(size_t i=0;i<plot->size;++i){
        low=std::min(low,plot->points[i].basis_points);
        high=std::max(high,plot->points[i].basis_points);any=true;
    }
    return heapScaleBounds(low,high,any);
}
/* Measured deck battery discharge slope; never fabricates an estimate. */
class BatteryTrend {
    static constexpr uint32_t kMinElapsedMs = 60000;
    static constexpr int kMinDropPct = 2;
    bool has_sample_ = false, has_anchor_ = false, charging_ = false;
    int pct_ = -1, anchor_pct_ = -1;
    uint32_t anchor_ms_ = 0, latest_ms_ = 0;
public:
    void observe(int battery_pct, bool is_charging, uint32_t now_ms)
    {
        charging_ = is_charging;
        if (battery_pct < 0 || battery_pct > 100) {
            has_sample_ = false;
            has_anchor_ = false;
            pct_ = -1;
            return;
        }
        if (is_charging) {
            has_sample_ = true;
            has_anchor_ = false;
            pct_ = battery_pct;
            latest_ms_ = now_ms;
            return;
        }
        if (!has_anchor_ || battery_pct > pct_) {
            anchor_pct_ = battery_pct;
            anchor_ms_ = now_ms;
            has_anchor_ = true;
        }
        has_sample_ = true;
        pct_ = battery_pct;
        latest_ms_ = now_ms;
    }
    bool valid() const { return has_sample_; }
    bool charging() const { return charging_; }
    int pct() const { return pct_; }
    bool hasEstimate() const
    {
        if (!has_anchor_ || charging_) return false;
        const uint32_t elapsed = latest_ms_ - anchor_ms_;
        return elapsed >= kMinElapsedMs && (anchor_pct_ - pct_) >= kMinDropPct;
    }
    /* Tenths of a percent per hour (e.g. 125 == 12.5%/h); 0 when unavailable. */
    uint32_t burnTenthPctPerHr() const
    {
        if (!hasEstimate()) return 0;
        const uint32_t elapsed = latest_ms_ - anchor_ms_;
        const uint64_t drop = static_cast<uint64_t>(anchor_pct_ - pct_);
        return static_cast<uint32_t>((drop * 36000000ULL + elapsed / 2) / elapsed);
    }
    /* Estimated minutes remaining at current measured slope; 0 when unavailable or 0%. */
    uint32_t estMinutes() const
    {
        if (!hasEstimate() || pct_ <= 0) return 0;
        const uint32_t elapsed = latest_ms_ - anchor_ms_;
        const uint64_t drop = static_cast<uint64_t>(anchor_pct_ - pct_);
        const uint64_t mins = (static_cast<uint64_t>(pct_) * elapsed) / (drop * 60000ULL);
        return mins > 9999ULL ? 9999U : static_cast<uint32_t>(mins);
    }
};

class SystemResources {
    HeapHistory deck_,probe_;
    BatteryTrend battery_;
    uint32_t last_deck_=0,last_probe_sequence_=0,last_probe_epoch_=0;
    uint64_t last_probe_uptime_=0;
    bool sampled_=false,have_probe_=false;
public:
    void observe(const SystemStatus &s,uint32_t now){
        if(!sampled_||now-last_deck_>=1000){
            deck_.push({now,s.deck_heap,s.deck_total,false});last_deck_=now;sampled_=true;
            battery_.observe(s.battery_pct,s.battery_charging,now);
        }
        if(s.link==ocp::LinkState::Ready&&s.probe_valid&&(!have_probe_||s.probe_sequence!=last_probe_sequence_)){
            probe_.push({s.probe_sample_ms,s.probe_heap,s.probe_total,have_probe_&&(s.probe_uptime<last_probe_uptime_||s.probe_epoch!=last_probe_epoch_)});
            last_probe_sequence_=s.probe_sequence;last_probe_uptime_=s.probe_uptime;last_probe_epoch_=s.probe_epoch;have_probe_=true;
        }
    }
    const HeapHistory &deck()const{return deck_;}
    const HeapHistory &probe()const{return probe_;}
    const BatteryTrend &battery()const{return battery_;}
};
}
