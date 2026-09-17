/*
 * lora_hex.c — bounded, allocation-free bytes-to-hex encoding for LoRa events.
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_hex.h"

bool lora_bytes_to_hex(const uint8_t *bytes, size_t len,
                       char *out, size_t out_size, size_t *required)
{
    if (len > (SIZE_MAX - 1u) / 2u) {
        if (required) *required = 0;
        return false;
    }

    const size_t needed = len * 2u + 1u;
    if (required) *required = needed;
    if ((len != 0 && !bytes) || !out || out_size < needed) return false;

    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2u] = digits[bytes[i] >> 4];
        out[i * 2u + 1u] = digits[bytes[i] & 0x0fu];
    }
    out[len * 2u] = '\0';
    return true;
}
