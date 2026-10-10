#include "model/drive_trail.h"
#include <cassert>
#include <limits>
#include <cstdio>
int main() {
 model::GnssFix fix; fix.valid=true; fix.lat_deg=45; fix.lon_deg=-122;
 model::DriveTrail t; t.observe(fix,false,1,1,false); assert(t.size()==0);
 t.observe(fix,true,2,2,false); t.observe(fix,true,2,3,false); assert(t.size()==1);
 fix.lat_deg+=.001; t.observe(fix,true,4,4,false); assert(t.size()==2 && t.distance()>100);
 const double distance=t.distance(); t.observe(fix,false,5,5,false);
 fix.lat_deg+=1; t.observe(fix,true,6,6,false); assert(t.point(2).gap && t.distance()==distance);
 t.observe(fix,true,7,7,true); assert(t.size()==1 && t.distance()==0);
 t.observe(fix,true,8,107,false); assert(t.cached() && t.duration(200)==100 && t.size()==1);
 t.observe(fix,true,9,300,false); assert(t.size()==1);
 fix.lat_deg=std::numeric_limits<double>::quiet_NaN(); t.observe(fix,true,10,400,true); assert(t.size()==0);
 fix.lat_deg=45;
 for(uint32_t i=0;i<100;++i) {fix.lat_deg+=.00001; t.observe(fix,true,100+i,1000+i,true);}
 assert(t.size()==64 && t.point(0).gap);
 model::DriveTrail wrap; wrap.observe(fix,true,1,0xfffffff0u,true); assert(wrap.duration(0x10u)==32);
 puts("Drive trail: stale/duplicate rejection, gaps, session reset/cache, bounds and clock wrap passed");
}
