/*
 * cc1101_regs.c — see cc1101_regs.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_regs.h"

uint32_t cc1101_freq_reg(uint32_t freq_hz, uint32_t f_osc_hz)
{
    return (uint32_t)(((uint64_t)freq_hz << 16) / f_osc_hz);
}

uint32_t cc1101_drate_reg(uint32_t target_baud, uint32_t f_osc_hz,
                          uint8_t *drate_e, uint8_t *drate_m)
{
    uint32_t best_rate = 0;
    uint8_t best_e = 0, best_m = 0;
    uint64_t best_err = (uint64_t)-1;

    for (int e = 0; e <= 15; e++) {
        for (int m = 0; m <= 255; m++) {
            uint64_t rate = (uint64_t)(256 + m) * ((uint64_t)1 << e) * f_osc_hz >> 28;
            uint64_t err = rate > target_baud ? rate - target_baud : target_baud - rate;
            if (err < best_err) {
                best_err = err;
                best_rate = (uint32_t)rate;
                best_e = (uint8_t)e;
                best_m = (uint8_t)m;
            }
        }
    }
    if (drate_e) *drate_e = best_e;
    if (drate_m) *drate_m = best_m;
    return best_rate;
}

uint32_t cc1101_chanbw_reg(uint32_t target_bw_hz, uint32_t f_osc_hz,
                           uint8_t *chanbw_e, uint8_t *chanbw_m)
{
    uint32_t best_bw = 0;
    uint8_t best_e = 0, best_m = 0;
    uint64_t best_err = (uint64_t)-1;

    for (int e = 0; e <= 3; e++) {
        for (int m = 0; m <= 3; m++) {
            uint32_t bw = (uint32_t)(f_osc_hz / (8ULL * (uint32_t)(4 + m) * (1u << e)));
            uint64_t err = bw > target_bw_hz ? (uint64_t)(bw - target_bw_hz) : (uint64_t)(target_bw_hz - bw);
            if (err < best_err) {
                best_err = err;
                best_bw = bw;
                best_e = (uint8_t)e;
                best_m = (uint8_t)m;
            }
        }
    }
    if (chanbw_e) *chanbw_e = best_e;
    if (chanbw_m) *chanbw_m = best_m;
    return best_bw;
}

int16_t cc1101_rssi_to_dbm(uint8_t raw, int rssi_offset_db)
{
    int16_t dec = raw;
    if (dec >= 128) dec -= 256;
    return (int16_t)(dec / 2 - rssi_offset_db);
}

int cc1101_drain_count(uint8_t first_rxbytes, uint8_t second_rxbytes, uint8_t max_chunk)
{
    uint8_t n1 = first_rxbytes & 0x7F;
    uint8_t n2 = second_rxbytes & 0x7F;
    int n;

    if (n1 == n2) {
        n = n1;   /* stable across two reads: safe to drain everything reported */
    } else {
        uint8_t lo = n1 < n2 ? n1 : n2;
        n = lo > 0 ? lo - 1 : 0;   /* still changing: drain conservatively, leave the rest for next poll */
    }
    if (n > (int)max_chunk) n = max_chunk;
    return n;
}
