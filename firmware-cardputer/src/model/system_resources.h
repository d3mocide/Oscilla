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
class SystemResources {
    HeapHistory deck_,probe_;
    uint32_t last_deck_=0,last_probe_sequence_=0,last_probe_epoch_=0;
    uint64_t last_probe_uptime_=0;
    bool sampled_=false,have_probe_=false;
public:
    void observe(const SystemStatus &s,uint32_t now){
        if(!sampled_||now-last_deck_>=1000){
            deck_.push({now,s.deck_heap,s.deck_total,false});last_deck_=now;sampled_=true;
        }
        if(s.link==ocp::LinkState::Ready&&s.probe_valid&&(!have_probe_||s.probe_sequence!=last_probe_sequence_)){
            probe_.push({s.probe_sample_ms,s.probe_heap,s.probe_total,have_probe_&&(s.probe_uptime<last_probe_uptime_||s.probe_epoch!=last_probe_epoch_)});
            last_probe_sequence_=s.probe_sequence;last_probe_uptime_=s.probe_uptime;last_probe_epoch_=s.probe_epoch;have_probe_=true;
        }
    }
    const HeapHistory &deck()const{return deck_;}
    const HeapHistory &probe()const{return probe_;}
};
}
