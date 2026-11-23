/*
 * cc1101_bench.h — bench-only CC1101 console on USB Serial/JTAG
 * (CONFIG_OSCILLA_CC1101_BENCH, D-15 requalification). Same line format and
 * commands as bench-cc1101-driver, plus `lload on|off` to load the shared
 * bus with SX1262 receive-only RSSI sweeps. Not an OCP surface.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_BENCH_H
#define OSCILLA_CC1101_BENCH_H

#include "esp_err.h"

/* Call after lora_recon_init(); starts the console task. */
esp_err_t cc1101_bench_start(void);

#endif /* OSCILLA_CC1101_BENCH_H */
