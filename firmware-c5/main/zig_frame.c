/* zig_frame.c — see zig_frame.h. */
#include "zig_frame.h"

#include <string.h>
#include "ocp.h"

static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

bool zig_frame_parse(const uint8_t *p, size_t n, zig_frame_t *out)
{
    if (!p || !out || n < 3) return false; /* byte 0 is PHY length */
    memset(out, 0, sizeof *out);
    size_t wire = p[0];
    if (wire < 3 || wire > 127 || wire + 1 > n) return false;
    uint16_t fcf = le16(p + 1);
    uint8_t type = fcf & 0x7;
    uint8_t dst_mode = (fcf >> 10) & 0x3;
    uint8_t src_mode = (fcf >> 14) & 0x3;
    bool pan_compress = (fcf & (1u << 6)) != 0;
    if (type > 3 || dst_mode == 1 || src_mode == 1) return false;
    size_t i = 4; /* length + FCF + sequence */
    if (dst_mode) {
        if (i + 2 > wire + 1) return false;
        out->pan_id = le16(p + i); out->has_pan = true; i += 2;
        if (dst_mode == 2) { if (i + 2 > wire + 1) return false; i += 2; }
        else { if (i + 8 > wire + 1) return false; i += 8; }
    }
    if (src_mode) {
        if (!pan_compress) { if (i + 2 > wire + 1) return false; out->pan_id = le16(p + i); out->has_pan = true; i += 2; }
        if (src_mode == 2) {
            if (i + 2 > wire + 1) return false;
            out->short_addr = le16(p + i); out->has_short = true;
        } else {
            if (i + 8 > wire + 1) return false;
            memcpy(out->ext_addr, p + i, 8); out->has_ext = true;
        }
    }
    out->frame_type = type;
    /* MAC alone cannot identify Zigbee or Thread; never guess. */
    out->proto = OCP_ZIG_PROTO_802154;
    return out->has_pan && (out->has_short || out->has_ext);
}
