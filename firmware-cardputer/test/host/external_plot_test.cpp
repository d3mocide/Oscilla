#include "ui/external_plot.h"
#include <cassert>
#include <cstdio>

int main()
{
    model::ChannelReading r;
    assert(ui::plot::scope(r, 60000, true).size == 0);
    r.history_size = r.history_next = 4;
    r.history[0] = {1, 1000};
    r.history[1] = {8, 2000};
    r.history[2] = {4, 6000};
    r.history[3] = {2, 7000};
    auto a = ui::plot::scope(r, 7100, true);
    assert(a.size == 4 && a.peak == 8 && a.scale == 8);
    assert(!a.points[0].connected && a.points[1].connected);
    assert(!a.points[2].connected && a.points[3].connected);
    assert(a.points[0].x < a.points[1].x && a.points[1].x < a.points[2].x);
    assert(a.points[1].y == 48);
    auto cached = ui::plot::scope(r, 100000, false);
    assert(cached.size == 4 && cached.points[3].x == 300);
    assert(ui::plot::scope(r, 100000, true).size == 0);
    r.history_size = 2; r.history_next = 1;
    r.history[31] = {UINT32_MAX, UINT32_MAX - 500};
    r.history[0] = {3, 500};
    a = ui::plot::scope(r, 600, true);
    assert(a.size == 2 && a.points[1].connected);
    assert(a.scale == UINT32_MAX && a.points[0].y == 48);
    assert(a.points[0].x < a.points[1].x);
    puts("external scope: empty, real gaps, cached freeze, ring order, overflow and clock wrap passed");
}
