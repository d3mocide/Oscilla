/* Host tests for wardrive_cache.c (WP-3). */
#include "wardrive_cache.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    wardrive_cache_entry_t buf[4];
    wardrive_cache_t cache;
    wardrive_cache_init(&cache, buf, 4, 30000u);

    const uint8_t m1[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    const uint8_t m2[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
    const uint8_t m3[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x03};
    const uint8_t m4[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x04};
    const uint8_t m5[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x05};

    /* First sighting always emits; duplicate within TTL is suppressed. */
    assert(wardrive_cache_should_emit(&cache, m1, -80, 1000u));
    assert(!wardrive_cache_should_emit(&cache, m1, -78, 5000u));
    assert(!wardrive_cache_should_emit(&cache, m1, -71, 29999u)); /* +9 dB < +10 dB jump */

    /* RSSI jump >= +10 dB re-emits immediately and resets emit timer/RSSI. */
    assert(wardrive_cache_should_emit(&cache, m1, -70, 10000u));
    assert(!wardrive_cache_should_emit(&cache, m1, -68, 20000u));

    /* TTL expiry re-emits cleanly (including across uint32_t clock wraparound). */
    assert(wardrive_cache_should_emit(&cache, m1, -72, 40000u));
    assert(wardrive_cache_should_emit(&cache, m2, -75, 0xFFFFF000u));
    assert(!wardrive_cache_should_emit(&cache, m2, -75, 0xFFFFF100u));
    assert(wardrive_cache_should_emit(&cache, m2, -75, 0x00007000u)); /* wrapped past +30 s */

    /* LRU eviction when full: oldest last_seen entry is replaced, never stalls. */
    wardrive_cache_reset(&cache, 30000u);
    assert(wardrive_cache_should_emit(&cache, m1, -70, 100u));
    assert(wardrive_cache_should_emit(&cache, m2, -70, 200u));
    assert(wardrive_cache_should_emit(&cache, m3, -70, 300u));
    assert(wardrive_cache_should_emit(&cache, m4, -70, 400u));
    /* Touch m1 so m2 becomes the least-recently-seen entry. */
    assert(!wardrive_cache_should_emit(&cache, m1, -70, 500u));
    /* Inserting m5 evicts m2 (LRU); m1 stays suppressed while m2 is now new again. */
    assert(wardrive_cache_should_emit(&cache, m5, -70, 600u));
    assert(cache.count == 4);
    assert(!wardrive_cache_should_emit(&cache, m1, -70, 700u));
    assert(wardrive_cache_should_emit(&cache, m2, -70, 800u));

    printf("wardrive cache: TTL dedup, RSSI jump re-emit, clock wrap and LRU eviction passed\n");
    return 0;
}
