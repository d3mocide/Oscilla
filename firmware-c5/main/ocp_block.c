/*
 * ocp_block.c — transactional bounded block-frame buffer.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_block.h"

#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"

/* A full 256-row frame needs up to ~66 KB. Internal RAM belongs to the radio
 * stacks, and its largest free block can be under 32 KB with every lane
 * live, so build frames in PSRAM; internal RAM is only the fallback. */
static void *block_realloc(void *ptr, size_t size)
{
    void *p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
#else
#define block_realloc realloc
#endif

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
        char *next = block_realloc(block->bytes, capacity);
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
