#include <cassert>
#include <cstdio>
#include "storage/sd_activity.h"

int main()
{
    storage::SdActivity a;
    assert(a.indicator(false, 0) == storage::SdIndicator::Off);
    assert(a.indicator(true, 0) == storage::SdIndicator::Ready);
    a.record(256, 256, 10);
    assert(a.writes == 1 && a.bytes == 256 && a.errors == 0);
    assert(a.indicator(true, 11) == storage::SdIndicator::Write);
    assert(a.indicator(true, 1510) == storage::SdIndicator::Ready);
    a.record(256, 12, 620);
    assert(a.writes == 1 && a.bytes == 268 && a.errors == 1);
    assert(a.indicator(true, 621) == storage::SdIndicator::Fault);
    a.record(0, 0, 700);
    assert(a.indicator(true, 701) == storage::SdIndicator::Fault);
    a.record(5, 5, 0xFFFFFFF0u);
    assert(a.writes == 2 && a.errors == 1);
    assert(a.indicator(true, 20) == storage::SdIndicator::Write);
    assert(a.indicator(true, 1600) == storage::SdIndicator::Ready);
    std::puts("SD activity: accepted writes, short-write fault, empty writes, wraparound OK");
}
