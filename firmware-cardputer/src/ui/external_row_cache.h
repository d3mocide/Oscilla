/* Physical-row diff cache for the external 320x240 companion display. */
#pragma once

#include <cstddef>
#include <cstdint>

namespace ui {
namespace detail {

class ExternalRowCache {
public:
    static constexpr size_t kHeight = 240;
    static constexpr size_t kTiles = 8, kTileWidth = 40;
    struct Span { size_t first = 0, count = 0; };
    static size_t planSpans(const bool *changed,Span (&out)[kTiles])
    {
        size_t used=0;
        for(size_t tile=0;tile<kTiles;) {
            if(!changed[tile]) {++tile;continue;}
            const size_t first=tile++;
            while(tile<kTiles&&changed[tile])++tile;
            out[used++]={first,tile-first};
        }
        return used;
    }

    bool needsTileUpdate(size_t y,size_t tile,uint64_t hash) const
    {
        return !valid_ || y>=kHeight || tile>=kTiles || tiles_[y][tile]!=hash;
    }
    void storeTile(size_t y,size_t tile,uint64_t hash)
    {
        if(y<kHeight && tile<kTiles)tiles_[y][tile]=hash;
    }

    bool needsUpdate(size_t screen_y, uint64_t hash) const
    {
        return !valid_ || screen_y >= kHeight || hashes_[screen_y] != hash;
    }

    void store(size_t screen_y, uint64_t hash)
    {
        if (screen_y < kHeight) hashes_[screen_y] = hash;
    }

    void invalidate() { valid_ = false; }
    void validate() { valid_ = true; }

private:
    uint64_t hashes_[kHeight]{};
    uint64_t tiles_[kHeight][kTiles]{};
    bool valid_ = false;
};

}  // namespace detail
}  // namespace ui
