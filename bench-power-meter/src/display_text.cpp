/*
 * display_text.cpp — see display_text.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "display_text.h"

#include <cinttypes>
#include <cstdio>

/* Lines are cut to the panel's 21 columns on purpose. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

namespace meter {

void formatMa(char *buf, size_t cap, int64_t ua)
{
    const char *sign = ua < 0 ? "-" : "";
    uint64_t a = static_cast<uint64_t>(ua < 0 ? -ua : ua);
    if (a < 100000)   /* < 100 mA: two decimals */
        std::snprintf(buf, cap, "%s%" PRIu64 ".%02" PRIu64, sign, a / 1000, (a % 1000) / 10);
    else
        std::snprintf(buf, cap, "%s%" PRIu64 ".%" PRIu64, sign, a / 1000, (a % 1000) / 100);
}

static void formatV(char *buf, size_t cap, int32_t mv)
{
    if (mv < 0) std::snprintf(buf, cap, "-.--");
    else std::snprintf(buf, cap, "%" PRId32 ".%02" PRId32, mv / 1000, (mv % 1000) / 10);
}

void composeLines(const DisplaySnapshot &s, DisplayLines &out)
{
    char v[12], vmin[12], a[16], lo[16], hi[16];
    formatV(v, sizeof v, s.vbus_mv);
    std::snprintf(out.head, kLineCap, "%-6s %sV %s", s.chip, v, s.probe_up ? "UP" : "DOWN");

    if (!s.valid) {
        std::snprintf(out.big, kLineCap, s.chip[0] == 'n' ? "NO SENSOR" : "--");
        std::snprintf(out.range, kLineCap, "%s", s.chip[0] == 'n' ? "check SDA D4 SCL D5" : "");
        out.flags[0] = '\0';
    } else {
        formatMa(a, sizeof a, s.mean_ua);
        formatMa(lo, sizeof lo, s.min_ua);
        formatMa(hi, sizeof hi, s.max_ua);
        std::snprintf(out.big, kLineCap, "%s mA", a);
        std::snprintf(out.range, kLineCap, "lo %s pk %s", lo, hi);
        formatV(vmin, sizeof vmin, s.vbus_min_mv);
        std::snprintf(out.flags, kLineCap, "Vmin %s%s%s%s", vmin, s.sag ? " SAG" : "",
                      s.clip ? " CLIP" : "", s.i2c_err ? " I2C!" : "");
    }

    if (s.label[0]) std::snprintf(out.label, kLineCap, "> %s", s.label);
    else std::snprintf(out.label, kLineCap, "%s", s.stream ? "streaming" : "idle");
}

}  // namespace meter
