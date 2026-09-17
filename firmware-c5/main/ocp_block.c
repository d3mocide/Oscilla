/*
 * ocp_block.c — transactional bounded block-frame buffer.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_block.h"

#include <stdlib.h>
#include <string.h>

void ocp_block_begin(ocp_block_t *block)
{
    block->len = 0;
    block->active = true;
    block->failed = false;
}

bool ocp_block_append(ocp_block_t *block, const char *bytes, size_t len, size_t max_bytes)
{
    if (!block->active || block->failed) return false;
    if (len > max_bytes - block->len) {
        block->failed = true;
        return false;
    }
    if (block->len + len > block->capacity) {
        size_t capacity = block->capacity ? block->capacity : 1024;
        while (capacity < block->len + len) {
            if (capacity > max_bytes / 2u) { capacity = max_bytes; break; }
            capacity *= 2u;
        }
        char *next = realloc(block->bytes, capacity);
        if (!next) {
            block->failed = true;
            return false;
        }
        block->bytes = next;
        block->capacity = capacity;
    }
    memcpy(block->bytes + block->len, bytes, len);
    block->len += len;
    return true;
}

bool ocp_block_commit(ocp_block_t *block, ocp_block_writer_fn writer, void *arg)
{
    if (!block->active || block->failed || !writer) return false;
    return writer(block->bytes, block->len, arg);
}

void ocp_block_discard(ocp_block_t *block)
{
    free(block->bytes);
    memset(block, 0, sizeof *block);
}
