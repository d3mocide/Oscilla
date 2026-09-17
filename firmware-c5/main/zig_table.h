/* zig_table.h — bounded PAN/node state for D-17. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ZIG_PANS_MAX 32
#define ZIG_NODES_MAX 96

typedef struct { uint16_t pan; const char *proto; uint16_t channels; uint16_t nodes; int8_t rssi; uint8_t lqi; } zig_pan_t;
typedef struct { uint16_t pan, short_addr; uint8_t ext[8]; bool has_short, has_ext; int8_t rssi; uint8_t lqi; uint32_t seen; } zig_node_t;
typedef struct { zig_pan_t pans[ZIG_PANS_MAX]; zig_node_t nodes[ZIG_NODES_MAX]; uint16_t pan_count, node_count, dropped; } zig_table_t;

void zig_table_clear(zig_table_t *t);
bool zig_table_upsert(zig_table_t *t, uint16_t pan, const char *proto, uint8_t ch, uint16_t short_addr, bool has_short,
                      const uint8_t ext[8], bool has_ext, int8_t rssi, uint8_t lqi, uint32_t seen);
