/*
 * cc1101_bench.h — bench-only CC1101 console on USB Serial/JTAG
 * (CONFIG_OSCILLA_CC1101_BENCH, D-15 requalification). Same line format and
 * commands as bench-cc1101-driver, plus `lload on|off` (SX1262 receive-only
 * RSSI sweeps as shared-bus load) and `mode fifo <baud>` / `mode async`
 * (demodulated OOK samples over SPI as `B,t_ms,n,hex` lines, drained by
 * cc1101_capture on a fixed tick; `drain N` sets the tick), decoded OOK
 * packets as `O,t_ms,proto,nbits,hex,frames=,te=,...` lines (`raw off` hides B
 * lines). Not OCP.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_BENCH_H
#define OSCILLA_CC1101_BENCH_H

#include "esp_err.h"

/* Call after lora_recon_init(); starts the console task. */
esp_err_t cc1101_bench_start(void);

#endif /* OSCILLA_CC1101_BENCH_H */
