/*
 * config.h — NVS-backed probe configuration persistence (DESIGN §6.1).
 *
 * Loads region, band scope, custom channels, and last LoRa RX config at boot
 * before radio init; best-effort commits on valid OCP configuration changes.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CONFIG_H
#define OSCILLA_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "config_codec.h"

/* Load persisted config from NVS (falling back to defaults on empty/corrupt)
 * and apply Wi-Fi region/band/channels into wifi_channels.c. Never fails boot. */
void probe_config_load(void);

/* Read-only view of the currently cached probe configuration. */
const probe_config_t *probe_config_get(void);

/* Snapshot live Wi-Fi region/band/channels from wifi_channels.c into NVS. */
bool probe_config_save_wifi(void);

/* Save validated LoRa RX parameters into NVS. */
bool probe_config_save_lora(uint32_t freq_hz, uint8_t sf, uint16_t bw_khz,
                            uint8_t cr, uint8_t sync_word);

#endif /* OSCILLA_CONFIG_H */
