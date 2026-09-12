/*
 * ocp_text.h — OCP field encoding, shared by both firmwares.
 *
 * Rules: OCP-SPEC.md §6. The reference implementation is tools/ocp.py;
 * protocol/test_ocp_text.c checks this agrees with it.
 *
 * Pure C99, no allocation. snprintf semantics throughout: the return value is
 * the length the output would have had, so a value >= out_size means the
 * result was truncated and must not be sent.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_TEXT_H
#define OSCILLA_OCP_TEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Quote and escape `len` bytes. Always safe to send: the result is printable
 * ASCII on one line whatever the input contained. */
size_t ocp_escape_field(const uint8_t *in, size_t len, char *out, size_t out_size);

/* Same, but leaves a value bare when the spec allows it (no quotes needed). */
size_t ocp_escape_value(const uint8_t *in, size_t len, char *out, size_t out_size);

/* True if `len` bytes need no quoting as a k=v value. */
int ocp_value_is_bare(const uint8_t *in, size_t len);

/* Decode one field back to bytes. Returns the decoded length, or -1 on a
 * malformed escape. Accepts the field with or without surrounding quotes. */
int ocp_unescape_field(const char *in, size_t len, uint8_t *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_OCP_TEXT_H */
