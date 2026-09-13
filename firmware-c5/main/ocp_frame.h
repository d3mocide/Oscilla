/*
 * ocp_frame.h — marker-frame emission (OCP-SPEC.md §3).
 *
 * Every emitter writes whole lines under a lock. A block frame also holds a
 * frame lock from BEGIN to END, so another task's reply cannot land between
 * its rows; [EVT] lines skip that lock, as the spec allows (OCP-SPEC §5.1).
 * Always pair ocp_emit_begin() with ocp_emit_end().
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_FRAME_H
#define OSCILLA_OCP_FRAME_H

#include "esp_err.h"

esp_err_t ocp_frame_init(void);

/* [TAG] k=v ... END on one line. */
void ocp_emit_compact(const char *tag, const char *fmt, ...);

/* [TAG] BEGIN k=v ... / [TAG] <row> / [TAG] END */
void ocp_emit_begin(const char *tag, const char *fmt, ...);
void ocp_emit_row(const char *tag, const char *fmt, ...);
void ocp_emit_end(const char *tag);

/* [EVT] kind=<kind> ... */
void ocp_emit_event(const char *kind, const char *fmt, ...);

/* [ERR] code=<code> msg="..." — msg is escaped, never format-injected. */
void ocp_emit_error(const char *code, const char *msg);

/* Bare replies with no marker; `pong` is the only one (OCP-SPEC §3.4). */
void ocp_emit_literal(const char *text);

#endif /* OSCILLA_OCP_FRAME_H */
