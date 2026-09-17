/* Host regression tests for strict command-number parsing. */
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "ocp_parse.h"

static unsigned passed;
static unsigned failed;

static void check(bool condition, const char *label)
{
    if (condition) ++passed;
    else { ++failed; fprintf(stderr, "FAIL: %s\n", label); }
}

int main(void)
{
    uint32_t value = 0;
    check(ocp_parse_u32("0", &value) && value == 0, "zero");
    check(ocp_parse_u32("60000", &value) && value == 60000, "ordinary value");
    check(ocp_parse_u32("4294967295", &value) && value == UINT32_MAX, "maximum uint32");
    check(!ocp_parse_u32("", &value), "empty input rejected");
    check(!ocp_parse_u32("+1", &value), "positive sign rejected");
    check(!ocp_parse_u32("-1", &value), "negative sign rejected");
    check(!ocp_parse_u32("11junk", &value), "suffix rejected");
    check(!ocp_parse_u32("4294967296", &value), "overflow rejected");
    check(!ocp_parse_u32(" 11", &value), "leading whitespace rejected");
    check(!ocp_parse_u32(NULL, &value), "null text rejected");
    check(!ocp_parse_u32("1", NULL), "null output rejected");

    printf("ocp parse test OK: %u passed, %u failed\n", passed, failed);
    return failed ? 1 : 0;
}
