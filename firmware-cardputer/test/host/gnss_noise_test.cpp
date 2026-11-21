#include "model/gnss_noise.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
 model::GnssFix f;f.valid=true;f.lat_deg=45;f.lon_deg=-122;f.utc="120000";
 model::GnssNoise n;n.start();n.observe(f,true,1);n.observe(f,true,2);assert(n.samples()==1);
 f.utc="120001";f.lat_deg+=10.0/111320;n.observe(f,true,3);
 f.utc="120002";f.lat_deg-=10.0/111320;n.observe(f,true,4);
 assert(n.samples()==3 && std::abs(n.path()-20)<.01 && std::abs(n.y())<.01);
 assert(n.rms()>4&&n.maxStep()>9);
 n.observe(f,false,5);f.utc="120003";f.lat_deg+=.01;n.observe(f,true,6);
 assert(std::abs(n.path()-20)<.01);
 n.stop();f.utc="120004";n.observe(f,true,7);assert(n.samples()==4);
 n.start();assert(n.samples()==0);f.lon_deg=181;n.observe(f,true,8);assert(n.samples()==0);
 f.lat_deg=0;f.lon_deg=179.999;f.utc="1";n.observe(f,true,9);
 f.lon_deg=-179.999;f.utc="2";n.observe(f,true,10);assert(n.x()>200&&n.x()<230);
 std::cout<<"GNSS noise: epoch dedup, stationary return, raw path, gaps, reset and dateline passed\n";
}
