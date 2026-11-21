/* Bounded real observations, with explicit time gaps and cached freeze. */
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace model {
struct SignalSample {uint32_t ms=0;int rssi=0;};
class SignalHistory {
    std::array<SignalSample,16> samples_{};size_t size_=0,next_=0;
public:
    void push(uint32_t ms,int rssi){samples_[next_]={ms,rssi};next_=(next_+1)%samples_.size();size_=std::min(size_+1,samples_.size());}
    size_t size()const{return size_;}
    const SignalSample &at(size_t i)const{return samples_[(next_+samples_.size()-size_+i)%samples_.size()];}
};
struct SignalPoint {uint32_t age=0;int rssi=0;bool connected=false;};
struct SignalPlot {std::array<SignalPoint,16> points{};size_t size=0;};
inline SignalPlot signalPlot(const SignalHistory &h,uint32_t now,bool live){
    SignalPlot out;if(!live&&h.size())now=h.at(h.size()-1).ms;
    uint32_t previous=0;bool have=false;
    for(size_t i=0;i<h.size();++i){const auto &s=h.at(i);uint32_t age=now-s.ms;
        if(age>120000){have=false;continue;}
        out.points[out.size++]={age,s.rssi,have&&s.ms-previous<=15000};previous=s.ms;have=true;
    }
    return out;
}
}
