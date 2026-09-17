/*
 * ocp_block.h — transactional bounded block-frame buffer.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_BLOCK_H
#define OSCILLA_OCP_BLOCK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *bytes;
    size_t len;
    size_t capacity;
    bool active;
    bool failed;
} ocp_block_t;

typedef bool (*ocp_block_writer_fn)(const char *bytes, size_t len, void *arg);

void ocp_block_begin(ocp_block_t *block);
bool ocp_block_append(ocp_block_t *block, const char *bytes, size_t len, size_t max_bytes);
bool ocp_block_commit(ocp_block_t *block, ocp_block_writer_fn writer, void *arg);
void ocp_block_discard(ocp_block_t *block);

#endif /* OSCILLA_OCP_BLOCK_H */
