/*
 * wardrive_cache.h — bounded TTL + LRU dedup cache for moving wardrives.
 *
 * Pure C, no ESP-IDF dependency; host-tested under ASan/UBSan.
 * Prevents UART flooding when stationary while allowing moving drives to
 * re-emit devices after `ttl_ms` or a significant RSSI increase, and evicts
 * the least-recently-seen entry when full so long drives never stall.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WARDRIVE_CACHE_H
#define OSCILLA_WARDRIVE_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WARDRIVE_RSSI_JUMP_DB 10

typedef struct {
    uint8_t  mac[6];
    int8_t   last_emit_rssi;
    uint8_t  used;
    uint32_t last_emit_ms;
    uint32_t last_seen_ms;
} wardrive_cache_entry_t;

typedef struct {
    wardrive_cache_entry_t *entries;
    size_t   capacity;
    size_t   count;
    uint32_t ttl_ms;
} wardrive_cache_t;

void wardrive_cache_init(wardrive_cache_t *c, wardrive_cache_entry_t *storage,
                         size_t capacity, uint32_t ttl_ms);
void wardrive_cache_reset(wardrive_cache_t *c, uint32_t ttl_ms);

/* Record an observation of `mac` at `rssi` and `now_ms`.
 * Returns true when the caller should emit an OCP observation event. */
bool wardrive_cache_should_emit(wardrive_cache_t *c, const uint8_t mac[6],
                                int8_t rssi, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_WARDRIVE_CACHE_H */
