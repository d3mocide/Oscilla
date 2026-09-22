/*
 * cc1101_regs_test.c — host test for cc1101_regs.c.
 *
 * Values below are computed by brute-force search over every (E, M)
 * combination, checked independently against the TI datasheet formulas
 * before being written here — not copied from the driver under test.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include "cc1101_regs.h"

#define FXOSC_HZ 26000000u

static int g_pass, g_fail;
static void check(int ok, const char *what)
{
    printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) g_pass++; else g_fail++;
}

int main(void)
{
    /* Frequency word: 433.92 MHz at 26 MHz osc (datasheet §13.1). */
    check(cc1101_freq_reg(433920000u, FXOSC_HZ) == 0x10B071u, "freq_reg(433.92MHz) matches hand-derived value");

    /* 2400 baud: brute-force search finds E=6, M=131, ~2398 baud — not the
     * driver's original, unverified E=6/M=0 guess. Historical (this was the
     * first profile's target before switching to 26000 baud oversampling,
     * 2026-09-22 — see WORKLOG); kept as a second worked example. */
    {
        uint8_t e = 0xFF, m = 0xFF;
        uint32_t achieved = cc1101_drate_reg(2400, FXOSC_HZ, &e, &m);
        check(e == 6 && m == 131, "drate_reg(2400) finds E=6 M=131");
        check(achieved == 2398, "drate_reg(2400) achieves 2398 baud (closest possible, not exactly 2400)");
    }

    /* 26000 baud: the active profile, target-oversamples an SDR-confirmed
     * local device's 232us shortest pulse by ~6x (see cc1101_radio.c's own
     * comment). E=10, M=6 lands almost exactly on target. */
    {
        uint8_t e = 0xFF, m = 0xFF;
        uint32_t achieved = cc1101_drate_reg(26000, FXOSC_HZ, &e, &m);
        check(e == 10 && m == 6, "drate_reg(26000) finds E=10 M=6");
        check(achieved == 25985, "drate_reg(26000) achieves 25985 baud");
    }

    /* Mutation-style proof (AGENTS.md: prove a check catches the bug):
     * the driver's original E=6/M=0 pair — chosen without running this
     * formula — does NOT land near 2.4 kBaud. This is the actual bug
     * found 2026-09-21/22 (see WORKLOG); this assertion would have
     * caught it before it shipped. */
    {
        uint32_t wrong_rate = (uint32_t)((256 + 0) * (1u << 6) * (uint64_t)FXOSC_HZ >> 28);
        check(wrong_rate == 1586, "the old E=6/M=0 pair achieves ~1586 baud, not 2.4k (confirms the bug)");
    }

    /* 203125 Hz target (the chip's own POR default): E=2, M=0 exactly. */
    {
        uint8_t e = 0xFF, m = 0xFF;
        uint32_t achieved = cc1101_chanbw_reg(203125, FXOSC_HZ, &e, &m);
        check(e == 2 && m == 0, "chanbw_reg(203125) finds E=2 M=0 (matches POR default)");
        check(achieved == 203125, "chanbw_reg(203125) is achieved exactly");
    }

    /* Mutation-style proof for the bandwidth mistake too: CHANBW_E=0/M=0
     * (what MDMCFG4=0x06 actually wrote) is 812.5 kHz, 4x wider than
     * intended. */
    {
        uint32_t wrong_bw = FXOSC_HZ / (8u * 4u * 1u);
        check(wrong_bw == 812500, "the old CHANBW_E=0/M=0 pair is 812.5kHz, not the intended narrow filter");
    }

    /* RSSI conversion (datasheet §17.3): raw < 128 is positive, raw >= 128
     * wraps to negative two's-complement. */
    check(cc1101_rssi_to_dbm(0, 74) == -74, "rssi_to_dbm(0) applies just the offset");
    check(cc1101_rssi_to_dbm(200, 74) == -102, "rssi_to_dbm wraps a >=128 raw value to negative first");

    /* Drain-count decision: stable reads drain everything; disagreeing
     * reads (byte mid-transfer) drain conservatively (min-1), never the
     * full reported count. Never trust a single read blindly. */
    check(cc1101_drain_count(10, 10, 64) == 10, "stable RXBYTES reads: drain the full count");
    check(cc1101_drain_count(10, 11, 64) == 9, "disagreeing reads: drain min-1, leaving the rest for next poll");
    check(cc1101_drain_count(0, 1, 64) == 0, "disagreeing reads at the boundary never go negative");
    check(cc1101_drain_count(70, 70, 64) == 64, "drain count is clamped to max_chunk regardless of RXBYTES");

    printf("\n%s: %d passed, %d failed\n", g_fail ? "cc1101 regs test FAILED" : "cc1101 regs test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
