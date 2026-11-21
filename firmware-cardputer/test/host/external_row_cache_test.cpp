/* Regression test for per-physical-row external display diffing. */
#include <cstdio>
#include <cassert>

#include "ui/external_row_cache.h"

namespace {
int passed, failed;
void check(bool ok, const char *name)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? passed : failed)++;
}
}

int main()
{
    for(unsigned mask=0;mask<256;++mask) {
        bool dirty[8];for(unsigned i=0;i<8;++i)dirty[i]=(mask>>i)&1;
        ui::detail::ExternalRowCache::Span spans[8];
        const size_t count=ui::detail::ExternalRowCache::planSpans(dirty,spans);
        unsigned covered=0;size_t end=0;
        for(size_t i=0;i<count;++i) {
            assert(spans[i].count>0&&spans[i].first>=end);
            end=spans[i].first+spans[i].count;assert(end<=8);
            for(size_t tile=spans[i].first;tile<end;++tile)covered|=1U<<tile;
        }
        assert(covered==mask);
        if(mask==3)assert(count==1&&spans[0].first==0&&spans[0].count==2);
        if(mask==5)assert(count==2);
        if(mask==9)assert(count==2);
    }
    {
        ui::detail::ExternalRowCache tiles;
        tiles.storeTile(12,3,100);
        tiles.validate();
        assert(!tiles.needsTileUpdate(12,3,100));
        assert(tiles.needsTileUpdate(12,4,100));
        assert(tiles.needsTileUpdate(36,3,100));
        tiles.invalidate();
        assert(tiles.needsTileUpdate(12,3,100));
    }
    ui::detail::ExternalRowCache cache;
    cache.invalidate();
    for (size_t y = 0; y < ui::detail::ExternalRowCache::kHeight; ++y)
        cache.store(y, y == 88 ? 2 : 1);
    cache.validate();

    /* Cursor moved from row 88 to row 40: both old and new rows must redraw. */
    check(cache.needsUpdate(88, 1),
          "clears the old highlight at its physical LCD row");
    check(cache.needsUpdate(40, 2),
          "draws the new highlight at its physical LCD row");
    check(!cache.needsUpdate(41, 1),
          "unchanged rows remain eligible for lazy skipping");

    cache.store(88, 1);
    cache.store(40, 2);
    check(!cache.needsUpdate(88, 1) && !cache.needsUpdate(40, 2),
          "committed redraws become the next frame baseline");

    cache.invalidate();
    check(cache.needsUpdate(41, 1),
          "page invalidation forces repaint even when pixels match");
    std::printf("external row cache: 256 span masks and %d row checks, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
