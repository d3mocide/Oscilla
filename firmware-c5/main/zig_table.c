/* zig_table.c — see zig_table.h. */
#include "zig_table.h"
#include <string.h>

void zig_table_clear(zig_table_t *t) { memset(t, 0, sizeof *t); }

static zig_pan_t *pan_for(zig_table_t *t, uint16_t pan)
{
    for (unsigned i = 0; i < t->pan_count; ++i) if (t->pans[i].pan == pan) return &t->pans[i];
    if (t->pan_count == ZIG_PANS_MAX) { ++t->dropped; return NULL; }
    zig_pan_t *p = &t->pans[t->pan_count++]; memset(p, 0, sizeof *p); p->pan = pan; return p;
}

bool zig_table_upsert(zig_table_t *t, uint16_t pan, const char *proto, uint8_t ch, uint16_t short_addr, bool has_short,
                      const uint8_t ext[8], bool has_ext, int8_t rssi, uint8_t lqi, uint32_t seen)
{
    zig_pan_t *p = pan_for(t, pan); if (!p) return false;
    p->proto = proto; p->channels |= (uint16_t)(1u << (ch - 11)); p->rssi = rssi; p->lqi = lqi;
    for (unsigned i = 0; i < t->node_count; ++i) {
        zig_node_t *n = &t->nodes[i];
        if (n->pan == pan && ((has_short && n->has_short && n->short_addr == short_addr) || (has_ext && n->has_ext && !memcmp(n->ext, ext, 8)))) {
            n->rssi = rssi; n->lqi = lqi; n->seen = seen; return false; /* update, not a first sighting */
        }
    }
    if (t->node_count == ZIG_NODES_MAX) { ++t->dropped; return false; }
    zig_node_t *n = &t->nodes[t->node_count++]; memset(n, 0, sizeof *n);
    n->pan = pan; n->short_addr = short_addr; n->has_short = has_short; n->has_ext = has_ext; n->rssi = rssi; n->lqi = lqi; n->seen = seen;
    if (has_ext) memcpy(n->ext, ext, 8);
    ++p->nodes;
    return true;
}
