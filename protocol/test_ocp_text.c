/*
 * test_ocp_text.c — emits `<hex>\t<field>\t<value>` lines for a corpus of
 * payloads, so tools/check_ocp_text.py can diff this C encoder against the
 * Python reference. Also asserts escape/unescape round-trips in C.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_text.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void emit(const uint8_t *in, size_t len)
{
    char esc[2048];
    uint8_t back[512];

    size_t n = ocp_escape_field(in, len, esc, sizeof esc);
    assert(n < sizeof esc);

    /* Escaped output is always one printable-ASCII line. */
    for (size_t i = 0; i < n; i++) {
        assert(esc[i] >= 0x20 && esc[i] <= 0x7E);
    }

    int got = ocp_unescape_field(esc, n, back, sizeof back);
    assert(got == (int)len);
    assert(memcmp(back, in, len) == 0);

    char val[2048];
    size_t vn = ocp_escape_value(in, len, val, sizeof val);
    assert(vn < sizeof val);
    for (size_t i = 0; i < vn; i++) {
        assert(val[i] >= 0x20 && val[i] <= 0x7E);   /* bare or not, one line */
    }

    for (size_t i = 0; i < len; i++) {
        printf("%02x", in[i]);
    }
    printf("\t%s\t%s\n", esc, val);   /* tab: never present in escaped output */
}

/* xorshift32: the Python side regenerates the identical corpus. */
static uint32_t rng_state = 0x05C111A5;

int main(void)
{
    /* Every single byte value. */
    for (unsigned b = 0; b < 256; b++) {
        uint8_t one = (uint8_t)b;
        emit(&one, 1);
    }

    /* Hand-picked shapes that have broken parsers before. */
    static const char *fixed[] = {
        "", "plain", "with space", "\"quoted\"", "back\\slash",
        "comma,separated", "newline\nhere", "cr\rhere", "tab\there",
        "caf\xc3\xa9", "\xff\xfe\xfd", "a\"b\\c,d e",
        "\x1b[31mANSI\x1b[0m", "]END[", "BEGIN", "END",
        "\n", "abc\n", "wifi24,wifi5\n", "1,6,11\r\n", "ok\x00",
    };
    for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++) {
        emit((const uint8_t *)fixed[i], strlen(fixed[i]));
    }

    /* Deterministic fuzz over full-range bytes. */
    for (int i = 0; i < 512; i++) {
        uint8_t buf[64];
        size_t len = (size_t)(rng_state % 33);
        for (size_t j = 0; j < len; j++) {
            rng_state ^= rng_state << 13;
            rng_state ^= rng_state >> 17;
            rng_state ^= rng_state << 5;
            buf[j] = (uint8_t)(rng_state & 0xFF);
        }
        emit(buf, len);
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 17;
        rng_state ^= rng_state << 5;
    }

    /* Truncation reports the needed length and never overflows. */
    uint8_t big[64];
    memset(big, 'A', sizeof big);
    char small[8];
    size_t need = ocp_escape_field(big, sizeof big, small, sizeof small);
    assert(need == sizeof big + 2);
    assert(strlen(small) < sizeof small);

    /* Bare-value rule matches the spec's charset. */
    assert(ocp_value_is_bare((const uint8_t *)"wifi24,wifi5", 12));
    assert(ocp_value_is_bare((const uint8_t *)"1,6,11", 6));
    assert(!ocp_value_is_bare((const uint8_t *)"has space", 9));
    assert(!ocp_value_is_bare((const uint8_t *)"", 0));

    /* Malformed escapes are rejected, not guessed at. */
    uint8_t out[16];
    assert(ocp_unescape_field("\\", 1, out, sizeof out) == -1);
    assert(ocp_unescape_field("\\q", 2, out, sizeof out) == -1);
    assert(ocp_unescape_field("\\xZZ", 4, out, sizeof out) == -1);
    assert(ocp_unescape_field("\\x4", 3, out, sizeof out) == -1);
    assert(ocp_unescape_field("\\x+f", 4, out, sizeof out) == -1);
    assert(ocp_unescape_field("\\x f", 4, out, sizeof out) == -1);

    return 0;
}
