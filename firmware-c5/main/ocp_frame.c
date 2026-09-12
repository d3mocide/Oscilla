/*
 * ocp_frame.c — see ocp_frame.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_frame.h"
#include "ocp_transport.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ocp.h"
#include "ocp_text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t s_lock;

esp_err_t ocp_frame_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

static void write_line(char *line, size_t cap, int n)
{
    if (n < 0 || n + 1 >= (int)cap) return;
    line[n++] = OCP_LINE_TERM_CHAR;

    if (s_lock && xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        ocp_transport_write(line, (size_t)n);
        xSemaphoreGive(s_lock);
    }
}

/* One line, written whole. Over-long output is dropped rather than sent
 * truncated: half a frame is worse than none (OCP-SPEC §1). */
static void emit_plain(const char *prefix, const char *suffix)
{
    char line[OCP_MAX_LINE_LEN + 2];
    int n = snprintf(line, sizeof line, "%s%s", prefix, suffix ? suffix : "");
    if (n >= (int)sizeof line) return;
    write_line(line, sizeof line, n);
}

static void emit(const char *prefix, const char *fmt, va_list ap, const char *suffix)
{
    char line[OCP_MAX_LINE_LEN + 2];
    int n = snprintf(line, sizeof line, "%s", prefix);
    if (n < 0 || n >= (int)sizeof line) return;

    if (fmt && *fmt) {
        int m = vsnprintf(line + n, sizeof line - (size_t)n, fmt, ap);
        if (m < 0 || n + m >= (int)sizeof line) return;
        n += m;
    }
    if (suffix && *suffix) {
        int m = snprintf(line + n, sizeof line - (size_t)n, "%s", suffix);
        if (m < 0 || n + m >= (int)sizeof line) return;
        n += m;
    }
    write_line(line, sizeof line, n);
}

static void emit_tagged(const char *tag, const char *mid, const char *fmt,
                        va_list ap, const char *suffix)
{
    char prefix[OCP_MAX_VERB_LEN + 8];
    snprintf(prefix, sizeof prefix, "%s %s", tag, mid);
    emit(prefix, fmt, ap, suffix);
}

void ocp_emit_compact(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char prefix[OCP_MAX_VERB_LEN + 2];
    snprintf(prefix, sizeof prefix, "%s ", tag);
    emit(prefix, fmt, ap, " " OCP_KW_END);
    va_end(ap);
}

void ocp_emit_begin(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_tagged(tag, OCP_KW_BEGIN " ", fmt, ap, "");
    va_end(ap);
}

void ocp_emit_row(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char prefix[OCP_MAX_VERB_LEN + 2];
    snprintf(prefix, sizeof prefix, "%s ", tag);
    emit(prefix, fmt, ap, "");
    va_end(ap);
}

void ocp_emit_end(const char *tag)
{
    char prefix[OCP_MAX_VERB_LEN + 8];
    snprintf(prefix, sizeof prefix, "%s %s", tag, OCP_KW_END);
    emit_plain(prefix, "");
}

void ocp_emit_event(const char *kind, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char prefix[64];
    snprintf(prefix, sizeof prefix, "%s %s=%s ", OCP_MARK_EVT, OCP_K_KIND, kind);
    emit(prefix, fmt, ap, "");
    va_end(ap);
}

void ocp_emit_error(const char *code, const char *msg)
{
    char escaped[256];
    ocp_escape_field((const uint8_t *)msg, strlen(msg), escaped, sizeof escaped);

    char prefix[OCP_MAX_LINE_LEN];
    snprintf(prefix, sizeof prefix, "%s %s=%s %s=%s",
             OCP_MARK_ERR, OCP_K_CODE, code, OCP_K_MSG, escaped);
    emit_plain(prefix, "");
}

void ocp_emit_literal(const char *text)
{
    emit_plain(text, "");
}
