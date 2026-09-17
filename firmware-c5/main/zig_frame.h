/* zig_frame.h — hostile 802.15.4 MAC-header parsing (D-17). */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t pan_id;
    uint16_t short_addr;
    uint8_t ext_addr[8];
    bool has_short;
    bool has_ext;
    bool has_pan;
    uint8_t frame_type;
    const char *proto;
} zig_frame_t;

/* Parses only the MAC header. It never interprets network-layer payloads. */
bool zig_frame_parse(const uint8_t *frame, size_t len, zig_frame_t *out);
