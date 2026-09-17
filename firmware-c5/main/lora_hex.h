/*
 * lora_hex.h — bounded, allocation-free bytes-to-hex encoding for LoRa events.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_LORA_HEX_H
#define OSCILLA_LORA_HEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* `required` receives the complete output capacity, including the NUL byte. */
bool lora_bytes_to_hex(const uint8_t *bytes, size_t len,
                       char *out, size_t out_size, size_t *required);

#endif /* OSCILLA_LORA_HEX_H */
