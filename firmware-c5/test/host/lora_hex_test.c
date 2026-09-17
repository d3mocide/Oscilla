/*
 * lora_hex_test.c — adversarial tests for the LoRa event hex encoder.
 * Build with -fsanitize=address,undefined.
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_hex.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

static char hex_digit(unsigned value)
{
    return value < 10 ? (char)('0' + value) : (char)('a' + value - 10);
}

static void known_answers(void)
{
    char out[2 * 255 + 2];
    size_t required = 0;

    memset(out, 'X', sizeof out);
    CHECK(lora_bytes_to_hex(NULL, 0, out, 1, &required), "zero-length payload encodes");
    CHECK(required == 1, "zero-length payload requires only its terminator");
    CHECK(out[0] == '\0' && out[1] == 'X', "zero-length output is empty and terminated");

    const uint8_t one[] = { 0x00 };
    memset(out, 'X', sizeof out);
    CHECK(lora_bytes_to_hex(one, sizeof one, out, 3, &required), "one-byte payload encodes");
    CHECK(required == 3 && strcmp(out, "00") == 0, "one-byte output has exact hex text");

    uint8_t max[255];
    for (size_t i = 0; i < sizeof max; i++) max[i] = (uint8_t)(i * 37u + 3u);
    memset(out, 'X', sizeof out);
    CHECK(lora_bytes_to_hex(max, sizeof max, out, sizeof out, &required), "maximum payload encodes");
    CHECK(required == 511 && out[510] == '\0' && out[511] == 'X',
          "maximum output is exactly sized and NUL-terminated");
    for (size_t i = 0; i < sizeof max; i++) {
        CHECK(out[2 * i] == hex_digit(max[i] >> 4) && out[2 * i + 1] == hex_digit(max[i] & 0xf),
              "maximum output preserves every byte");
    }
}

static void bounded_failures(void)
{
    const uint8_t bytes[] = { 0xab, 0xcd };
    char out[5];
    size_t required = 0;

    memset(out, 'X', sizeof out);
    CHECK(!lora_bytes_to_hex(bytes, sizeof bytes, out, sizeof out - 1, &required),
          "undersized output is rejected");
    CHECK(required == 5 && memcmp(out, "XXXXX", sizeof out) == 0,
          "undersized output is not partially written");

    CHECK(!lora_bytes_to_hex(NULL, 1, out, sizeof out, &required),
          "missing bytes are rejected for nonzero length");
    CHECK(required == 3, "required size is still reported on invalid input");
}

int main(void)
{
    known_answers();
    bounded_failures();
    printf("\n%s: %d passed, %d failed\n", g_fail ? "lora hex test FAILED" : "lora hex test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
