/*
 * lora_recon.h — RX survey on top of lora_radio.c: OCP verbs, framing, and
 * the [EVT] kind=lora stream (DESIGN §6.1). No transmit-shaped entry point.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_LORA_RECON_H
#define OSCILLA_LORA_RECON_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t lora_recon_init(void);

/* Caps are a promise (OCP-SPEC §4): only true once lora_radio_init() has
 * actually succeeded. */
bool lora_recon_ready(void);

void lora_cmd_config(int argc, char **argv);
void lora_cmd_listen(void);
void lora_cmd_status(void);

/* Idempotent; safe to call whether or not RX is running. True if it was. */
bool lora_cmd_stop(void);

#endif /* OSCILLA_LORA_RECON_H */
