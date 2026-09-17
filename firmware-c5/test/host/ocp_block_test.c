/*
 * ocp_block_test.c — host tests for transactional block-frame buffering.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_block.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

typedef struct {
    char bytes[128];
    size_t len;
    bool accept;
    unsigned calls;
} sink_t;

static bool write_sink(const char *bytes, size_t len, void *arg)
{
    sink_t *sink = arg;
    sink->calls++;
    if (!sink->accept || len > sizeof sink->bytes) return false;
    memcpy(sink->bytes, bytes, len);
    sink->len = len;
    return true;
}

int main(void)
{
    ocp_block_t block = {0};
    sink_t sink = { .accept = true };

    ocp_block_begin(&block);
    CHECK(ocp_block_append(&block, "[SCAN] BEGIN n=1\n", 17, 128), "begin is buffered");
    CHECK(ocp_block_append(&block, "[SCAN] row\n", 11, 128), "row is buffered");
    CHECK(sink.calls == 0, "transport is untouched before commit");
    CHECK(ocp_block_commit(&block, write_sink, &sink), "complete frame commits");
    CHECK(sink.calls == 1 && sink.len == 28 && !memcmp(sink.bytes, "[SCAN] BEGIN n=1\n[SCAN] row\n", sink.len),
          "commit writes one complete frame");
    ocp_block_discard(&block);

    sink = (sink_t){ .accept = true };
    ocp_block_begin(&block);
    CHECK(!ocp_block_append(&block, "partial", 7, 6), "oversized frame is rejected");
    CHECK(!ocp_block_commit(&block, write_sink, &sink), "rejected frame never commits");
    CHECK(sink.calls == 0, "rejected frame never touches transport");
    ocp_block_discard(&block);

    sink = (sink_t){ .accept = false };
    ocp_block_begin(&block);
    CHECK(ocp_block_append(&block, "[ERR]\n", 6, 128), "error frame buffers");
    CHECK(!ocp_block_commit(&block, write_sink, &sink), "unavailable transport reports failure");
    CHECK(sink.calls == 1 && sink.len == 0, "failed transport does not report a partial frame");
    ocp_block_discard(&block);

    printf("\n%s: %d passed, %d failed\n", g_fail ? "ocp block test FAILED" : "ocp block test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
