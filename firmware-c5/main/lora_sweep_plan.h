/*
 * lora_sweep_plan.h — pure-C SX1262 sub-GHz RSSI sweep validation & formatting
 * (WP-8). No ESP-IDF dependency; host-tested under ASan/UBSan.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_LORA_SWEEP_PLAN_H
#define OSCILLA_LORA_SWEEP_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LORA_SWEEP_DEF_START_KHZ  902000u
#define LORA_SWEEP_DEF_STEP_KHZ   400u
#define LORA_SWEEP_DEF_BINS       64u

#define LORA_SWEEP_MIN_FREQ_KHZ   150000u
#define LORA_SWEEP_MAX_FREQ_KHZ   960000u
#define LORA_SWEEP_MIN_STEP_KHZ   50u
#define LORA_SWEEP_MAX_STEP_KHZ   2000u
#define LORA_SWEEP_MIN_BINS       8u

/* Validate SX1262 frequency span without integer overflow. */
bool lora_sweep_plan_valid(uint32_t start_khz, uint32_t step_khz, uint32_t bins);

/* SX1262 datasheet §13.5.3: GetRssiInst raw byte -> signed dBm (-raw/2). */
int8_t lora_rssi_inst_to_dbm(uint8_t raw);

/* Format `n` signed dBm values as comma-separated ASCII into `out`. */
bool lora_sweep_format_rssi(const int8_t *rssi, size_t n, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_LORA_SWEEP_PLAN_H */
