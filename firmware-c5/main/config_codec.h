/*
 * config_codec.h — pure-C probe NVS configuration blob codec and validator
 * (DESIGN §6.1). No ESP-IDF dependency; host-tested under ASan/UBSan.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CONFIG_CODEC_H
#define OSCILLA_CONFIG_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROBE_CONFIG_BLOB_SIZE     128u
#define PROBE_CONFIG_REGION_MAX    8u
#define PROBE_CONFIG_CHANNELS_MAX  96u

typedef struct {
    char     wifi_region[PROBE_CONFIG_REGION_MAX];     /* ocp_wifi_region_t code */
    uint8_t  wifi_band;                                /* wifi_band_scope_t (0..2) */
    char     wifi_channels[PROBE_CONFIG_CHANNELS_MAX]; /* "" or "all", or CSV */
    uint8_t  lora_valid;                               /* 0=unset, 1=valid */
    uint32_t lora_freq_hz;                             /* 150000000..960000000 */
    uint8_t  lora_sf;                                  /* 5..12 */
    uint16_t lora_bw_khz;                              /* valid SX1262 BW kHz */
    uint8_t  lora_cr;                                  /* 1..4 */
    uint8_t  lora_sync_word;                           /* 0..255 */
} probe_config_t;

/* Populate safe compile-time defaults (US, auto, all channels, LoRa unset). */
void probe_config_defaults(probe_config_t *cfg);

/* True iff all fields are NUL-terminated and within hardware/regulatory bounds. */
bool probe_config_validate(const probe_config_t *cfg);

/* Encode validated config into a fixed 128-byte versioned, CRC32-protected blob. */
bool probe_config_encode(const probe_config_t *cfg, uint8_t out[PROBE_CONFIG_BLOB_SIZE]);

/* Decode and validate a blob. On any failure, populates defaults and returns false. */
bool probe_config_decode(const uint8_t *blob, size_t len, probe_config_t *out);

#endif /* OSCILLA_CONFIG_CODEC_H */
