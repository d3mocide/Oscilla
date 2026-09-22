/*
 * cc1101_regs.h — pure CC1101 register-value calculations, no SPI/GPIO/
 * FreeRTOS dependencies (same split as lora_hex.h). Host-tested in
 * firmware-c5/test/host/cc1101_regs_test.c.
 *
 * Formulas transcribed from the TI CC1101 datasheet (SWRS061), same
 * discipline lora_radio.c holds itself to. A prior version of
 * cc1101_radio.c hand-picked MDMCFG4/MDMCFG3 without running these
 * formulas and got both the data rate and the channel bandwidth wrong —
 * see WORKLOG 2026-09-21/22.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_REGS_H
#define OSCILLA_CC1101_REGS_H

#include <stdint.h>

/* RF frequency word, FREQ2:FREQ1:FREQ0 (datasheet §13.1):
 * FREQ[23:0] = freq_hz * 2^16 / f_osc_hz. */
uint32_t cc1101_freq_reg(uint32_t freq_hz, uint32_t f_osc_hz);

/* Data rate (datasheet §13.5):
 * DRATE = (256+DRATE_M) * 2^DRATE_E * f_osc_hz / 2^28.
 * Searches every (E, M) pair for the closest achievable rate to
 * target_baud, writes the register fields, and returns the rate actually
 * achieved (never exactly the target — log the truth, not the intent). */
uint32_t cc1101_drate_reg(uint32_t target_baud, uint32_t f_osc_hz,
                          uint8_t *drate_e, uint8_t *drate_m);

/* Channel filter bandwidth (datasheet §13.6):
 * BW = f_osc_hz / (8 * (4+CHANBW_M) * 2^CHANBW_E).
 * Same closest-match search over the 16 (E, M) combinations; returns the
 * achieved bandwidth in Hz. */
uint32_t cc1101_chanbw_reg(uint32_t target_bw_hz, uint32_t f_osc_hz,
                           uint8_t *chanbw_e, uint8_t *chanbw_m);

/* RSSI register to dBm (datasheet §17.3): two's-complement, then a
 * data-rate/filter-bandwidth-dependent offset (Table 31). */
int16_t cc1101_rssi_to_dbm(uint8_t raw, int rssi_offset_db);

/* How many bytes are safe to drain from the RX FIFO given two consecutive
 * RXBYTES readings (datasheet's own documented race: reading RXBYTES while
 * a byte is mid-transfer into the FIFO can return a stale/wrong count —
 * TI's guidance is to read until two consecutive reads agree, or drain
 * n-1 and leave the rest for the next poll). Bounded: this takes exactly
 * two readings, never loops. `overflow` reports the RXBYTES overflow flag
 * from the *first* reading (bit 7, datasheet §10.4) — the caller should
 * treat that as authoritative regardless of the second reading. */
int cc1101_drain_count(uint8_t first_rxbytes, uint8_t second_rxbytes, uint8_t max_chunk);

#endif /* OSCILLA_CC1101_REGS_H */
