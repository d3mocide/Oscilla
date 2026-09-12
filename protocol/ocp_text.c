/*
 * ocp_text.c — see ocp_text.h. Rules: OCP-SPEC.md §6.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_text.h"

static const char k_hex[] = "0123456789abcdef";

/* Bare values may carry the comma so caps= and channel lists need no quotes. */
static int bare_byte(uint8_t b)
{
    return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') ||
           (b >= '0' && b <= '9') ||
           b == '_' || b == '.' || b == ':' || b == '+' || b == ',' || b == '-';
}

int ocp_value_is_bare(const uint8_t *in, size_t len)
{
    if (len == 0) {
        return 0;   /* an empty value must be quoted to stay visible */
    }
    for (size_t i = 0; i < len; i++) {
        if (!bare_byte(in[i])) {
            return 0;
        }
    }
    return 1;
}

/* Append one char, tracking the length that would be needed. */
static void put(char *out, size_t out_size, size_t *n, char c)
{
    if (*n + 1 < out_size) {
        out[*n] = c;
    }
    (*n)++;
}

size_t ocp_escape_field(const uint8_t *in, size_t len, char *out, size_t out_size)
{
    size_t n = 0;

    put(out, out_size, &n, '"');
    for (size_t i = 0; i < len; i++) {
        uint8_t b = in[i];
        if (b == '\\' || b == '"') {
            put(out, out_size, &n, '\\');
            put(out, out_size, &n, (char)b);
        } else if (b >= 0x20 && b <= 0x7E) {
            put(out, out_size, &n, (char)b);
        } else {
            put(out, out_size, &n, '\\');
            put(out, out_size, &n, 'x');
            put(out, out_size, &n, k_hex[b >> 4]);
            put(out, out_size, &n, k_hex[b & 0x0F]);
        }
    }
    put(out, out_size, &n, '"');

    if (out_size > 0) {
        out[n < out_size ? n : out_size - 1] = '\0';
    }
    return n;
}

size_t ocp_escape_value(const uint8_t *in, size_t len, char *out, size_t out_size)
{
    if (!ocp_value_is_bare(in, len)) {
        return ocp_escape_field(in, len, out, out_size);
    }

    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        put(out, out_size, &n, (char)in[i]);
    }
    if (out_size > 0) {
        out[n < out_size ? n : out_size - 1] = '\0';
    }
    return n;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int ocp_unescape_field(const char *in, size_t len, uint8_t *out, size_t out_size)
{
    if (len >= 2 && in[0] == '"' && in[len - 1] == '"') {
        in += 1;
        len -= 2;
    }

    size_t n = 0;
    for (size_t i = 0; i < len; ) {
        if (in[i] != '\\') {
            if (n >= out_size) return -1;
            out[n++] = (uint8_t)in[i++];
            continue;
        }
        if (i + 1 >= len) return -1;
        char esc = in[i + 1];
        if (esc == 'x') {
            if (i + 3 >= len) return -1;
            int hi = hex_val(in[i + 2]), lo = hex_val(in[i + 3]);
            if (hi < 0 || lo < 0) return -1;
            if (n >= out_size) return -1;
            out[n++] = (uint8_t)((hi << 4) | lo);
            i += 4;
        } else if (esc == '"' || esc == '\\') {
            if (n >= out_size) return -1;
            out[n++] = (uint8_t)esc;
            i += 2;
        } else {
            return -1;
        }
    }
    return (int)n;
}
