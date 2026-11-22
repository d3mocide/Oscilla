/*
 * wardrive_cache.c — see wardrive_cache.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wardrive_cache.h"

#include <string.h>

void wardrive_cache_init(wardrive_cache_t *c, wardrive_cache_entry_t *storage,
                         size_t capacity, uint32_t ttl_ms)
{
    if (!c) return;
    c->entries = storage;
    c->capacity = storage ? capacity : 0;
    c->count = 0;
    c->ttl_ms = ttl_ms;
    if (c->entries && c->capacity > 0) {
        memset(c->entries, 0, c->capacity * sizeof *c->entries);
    }
}

void wardrive_cache_reset(wardrive_cache_t *c, uint32_t ttl_ms)
{
    if (!c) return;
    c->count = 0;
    c->ttl_ms = ttl_ms;
    if (c->entries && c->capacity > 0) {
        memset(c->entries, 0, c->capacity * sizeof *c->entries);
    }
}

bool wardrive_cache_should_emit(wardrive_cache_t *c, const uint8_t mac[6],
                                int8_t rssi, uint32_t now_ms)
{
    if (!c || !c->entries || c->capacity == 0 || !mac) return false;

    size_t lru_idx = 0;
    uint32_t lru_age = 0;

    for (size_t i = 0; i < c->count; i++) {
        wardrive_cache_entry_t *e = &c->entries[i];
        if (memcmp(e->mac, mac, 6) == 0) {
            e->last_seen_ms = now_ms;
            uint32_t elapsed = now_ms - e->last_emit_ms;
            bool ttl_expired = (c->ttl_ms == 0) || (elapsed >= c->ttl_ms);
            bool stronger = ((int)rssi - (int)e->last_emit_rssi) >= WARDRIVE_RSSI_JUMP_DB;
            if (ttl_expired || stronger) {
                e->last_emit_ms = now_ms;
                e->last_emit_rssi = rssi;
                return true;
            }
            return false;
        }
        uint32_t age = now_ms - e->last_seen_ms;
        if (i == 0 || age > lru_age) {
            lru_age = age;
            lru_idx = i;
        }
    }

    size_t slot = (c->count < c->capacity) ? c->count++ : lru_idx;
    wardrive_cache_entry_t *dst = &c->entries[slot];
    memcpy(dst->mac, mac, 6);
    dst->last_emit_rssi = rssi;
    dst->used = 1;
    dst->last_emit_ms = now_ms;
    dst->last_seen_ms = now_ms;
    return true;
}
