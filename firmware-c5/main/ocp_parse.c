/* ocp_parse.c — see ocp_parse.h. */
#include "ocp_parse.h"

#include <limits.h>

bool ocp_parse_u32(const char *text, uint32_t *out)
{
    if (!text || !*text || !out) return false;

    uint32_t value = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    *out = value;
    return true;
}
