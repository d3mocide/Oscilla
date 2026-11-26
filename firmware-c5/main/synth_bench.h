/*
 * synth_bench.h — bench-only synthetic-event console on USB Serial/JTAG
 * (CONFIG_OSCILLA_SYNTH_BENCH). OCP stays on the Grove UART; this console
 * injects made-up `network` and `ble` events into the probe's normal event
 * queue so the deck's tables, caps and wardrive log can be driven past their
 * limits on the bench. Nothing is transmitted: the events only go to the deck
 * over the wire, exactly as real ones do. Adds no OCP verb.
 *
 * Commands (one per line):
 *   synth <wifi_n> <ble_n> <seconds> [keep]
 *       Introduce wifi_n networks and ble_n devices, spread evenly over
 *       <seconds>. The first `keep` of each lane (default 8) are re-reported
 *       every 5 s for the whole run, so they stay "still heard".
 *   stop     end a run early
 *   status   one progress line
 * Output: `S,<what>,key=value,...` lines. Addresses: see synth_plan.h.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_SYNTH_BENCH_H
#define OSCILLA_SYNTH_BENCH_H

#include "esp_err.h"

esp_err_t synth_bench_start(void);

#endif /* OSCILLA_SYNTH_BENCH_H */
