#include "model/system_resources.h"
#include <cassert>
#include <iostream>
int main(){
    model::SystemResources m;model::SystemStatus s;
    s.deck_heap=100;s.deck_total=200;s.link=ocp::LinkState::Ready;
    s.probe_valid=true;s.probe_heap=50;s.probe_total=200;
    s.probe_sequence=1;s.probe_sample_ms=1000;s.probe_uptime=1000;
    m.observe(s,1000);m.observe(s,1500);
    assert(m.deck().size()==1 && m.probe().size()==1);
    m.observe(s,2000);assert(m.deck().size()==2 && m.probe().size()==1);
    auto plot=model::heapPlot(m.probe(),3000,4500);
    assert(plot.size==1 && plot.points[0].percent==25);
    s.probe_sequence=2;s.probe_sample_ms=3000;s.probe_uptime=3000;m.observe(s,3000);
    assert(model::heapPlot(m.probe(),3000,4500).points[1].connected);
    s.probe_sequence=3;s.probe_sample_ms=10000;s.probe_uptime=10000;m.observe(s,10000);
    assert(!model::heapPlot(m.probe(),10000,4500).points[2].connected);
    s.probe_sequence=4;s.probe_sample_ms=11000;s.probe_uptime=10;m.observe(s,11000);
    assert(!model::heapPlot(m.probe(),11000,4500).points[3].connected);
    s.probe_sequence=5;s.probe_sample_ms=12000;s.probe_uptime=20000;s.probe_epoch=1;m.observe(s,12000);
    assert(!model::heapPlot(m.probe(),12000,4500).points[4].connected);
    s.link=ocp::LinkState::Disconnected;s.probe_sequence=5;m.observe(s,12000);
    assert(m.probe().size()==5);
    assert(model::heapPlot(m.probe(),100000,4500).size==0);
    for(unsigned i=0;i<100;i++)m.observe(s,13000+i*1000);
    assert(m.deck().size()==61);
    model::HeapHistory wrap;wrap.push({UINT32_MAX-500,UINT32_MAX,UINT32_MAX,false});wrap.push({500,1,4,false});
    auto p=model::heapPlot(wrap,600,1600);assert(p.size==2 && p.points[1].connected && p.points[0].percent==100);
    wrap.push({700,10,0,false});assert(model::heapPlot(wrap,800,1600).size==2);
    model::SystemResources cold;model::SystemStatus absent;absent.deck_total=100;
    cold.observe(absent,0);cold.observe(absent,999);
    assert(cold.deck().size()==1 && cold.probe().size()==0);
    cold.observe(absent,1000);assert(cold.deck().size()==2);
    model::HeapHistory interrupted;interrupted.push({1000,50,100,false});
    interrupted.push({2000,50,0,false});interrupted.push({3000,50,100,false});
    assert(!model::heapPlot(interrupted,3000,4500).points[1].connected);
    wrap.push({900,1000,100,false});assert(model::heapPlot(wrap,1000,1600).points[2].percent==100);
    model::HeapHistory small;small.push({1000,5001,10000,false});small.push({2000,5099,10000,false});
    auto fine=model::heapPlot(small,2000,1600);
    assert(fine.points[0].basis_points==5001 && fine.points[1].basis_points==5099);
    auto zoom=model::heapScale(fine,{});
    assert(zoom.high-zoom.low>=1000 && zoom.low<=5001 && zoom.high>=5099);
    assert(zoom.y(5099)<zoom.y(5001));
    assert(zoom.y(0)==131 && zoom.y(10000)==53);
    model::HeapHistory edges;edges.push({1000,0,100,false});edges.push({2000,100,100,false});
    auto full=model::heapScale(model::heapPlot(edges,2000,1600),fine);
    assert(full.low==0 && full.high==10000);
    model::HeapHistory high;high.push({1000,9999,10000,false});
    auto ceiling=model::heapScale(model::heapPlot(high,1000,1600),{});
    assert(ceiling.high==10000 && ceiling.high-ceiling.low>=1000);
    auto empty=model::heapScale({},{});assert(empty.low==0 && empty.high==10000);
    for(unsigned a=0;a<=10000;a+=137)for(unsigned b=0;b<=10000;b+=211){
        model::HeapPlot one,two;one.size=two.size=1;
        one.points[0].basis_points=a;two.points[0].basis_points=b;
        const auto axis=model::heapScale(one,two);
        assert(axis.low<=std::min(a,b)&&axis.high>=std::max(a,b));
        assert(axis.high<=10000&&axis.high-axis.low>=1000);
        assert(axis.low%500==0&&axis.high%500==0);
    }
    model::HeapHistory stream;stream.push({UINT32_MAX-1000,25,100,false});stream.push({1000,50,100,false});stream.push({9000,10,100,true});
    std::array<model::HeapPoint,3> visited{};size_t n=0;
    model::visitHeapPoints(stream,10000,4500,[&](const model::HeapPoint &point){visited[n++]=point;});
    assert(n==3&&visited[0].age==11001&&visited[0].basis_points==2500&&!visited[0].connected);
    assert(visited[1].age==9000&&visited[1].basis_points==5000&&visited[1].connected);
    assert(visited[2].age==1000&&visited[2].basis_points==1000&&!visited[2].connected);
    std::cout<<"system resources: cadence, deduplication, gaps, reboot, disconnect, bounds and clock wrap and automatic scale passed\n";
}
