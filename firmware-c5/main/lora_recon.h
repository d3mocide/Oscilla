/*
 * lora_recon.h — RX survey on top of lora_radio.c: OCP verbs, framing, and
 * the [EVT] kind=lora stream (DESIGN §6.1). No transmit-shaped entry point.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_LORA_RECON_H
#define OSCILLA_LORA_RECON_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t lora_recon_init(void);

/* Seed RX parameters restored from NVS before OCP starts (never starts RX). */
bool lora_recon_apply_saved(uint32_t freq_hz, uint8_t sf, uint16_t bw_khz,
                            uint8_t cr, uint8_t sync_word);

/* Caps are a promise (OCP-SPEC §4): only true once lora_radio_init() has
 * actually succeeded. */
bool lora_recon_ready(void);

void lora_cmd_config(int argc, char **argv);
void lora_cmd_listen(void);
void lora_cmd_status(void);

/* Idempotent; safe to call whether or not RX is running. True if it was. */
bool lora_cmd_stop(void);

#endif /* OSCILLA_LORA_RECON_H */
