/*
 * cc1101_test.c — adversarial tests for the CC1101 bus, profile and config
 * layers against cc1101_sim. Build with -fsanitize=address,undefined.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_bus.h"
#include "cc1101_config.h"
#include "cc1101_profile.h"
#include "cc1101_sim.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

#define WRITABLE_COUNT (CC1101_CFG_COUNT - 3)

static void fresh(cc1101_sim_t *sim, cc1101_bus_t *bus)
{
    cc1101_sim_init(sim);
    *bus = cc1101_sim_bus(sim);
}

static void profile_data(void)
{
    const cc1101_profile_t *p = &cc1101_profile_ook_433_async;
    const uint32_t hz = cc1101_profile_freq_hz(p);
    const uint32_t step = p->fxosc_hz >> 16;
    CHECK(hz > 433920000u - step && hz < 433920000u + step, "profile is tuned to 433.92 MHz");
    CHECK(((p->reg[CC1101_MCSM0] >> 4) & 0x3) == 1, "profile autocalibrates IDLE->RX");
    CHECK(p->reg[CC1101_IOCFG1] == 0x2E, "GDO1/SO is tri-stated for a shared bus");
    CHECK(p->reg[CC1101_IOCFG0] != 0x3F, "GDO0 is not left on the POR clock output");

    CHECK(!cc1101_profile_writable(CC1101_FSTEST), "FSTEST is never written");
    CHECK(!cc1101_profile_writable(CC1101_PTEST), "PTEST is never written");
    CHECK(!cc1101_profile_writable(CC1101_AGCTEST), "AGCTEST is never written");
    CHECK(cc1101_profile_writable(0x2C) && cc1101_profile_writable(0x2E), "TEST2..TEST0 are written");
    CHECK(!cc1101_profile_writable(CC1101_CFG_COUNT), "nothing past 0x2E is a register");

    cc1101_profile_t q = *p;
    CHECK(cc1101_profile_set_freq(&q, 315000000u), "315 MHz is in band");
    const uint32_t got = cc1101_profile_freq_hz(&q);
    CHECK(got > 315000000u - step && got < 315000000u + step, "315 MHz round-trips within one step");
    const cc1101_profile_t before = q;
    CHECK(!cc1101_profile_set_freq(&q, 360000000u), "360 MHz (band gap) is refused");
    CHECK(!cc1101_profile_set_freq(&q, 950000000u), "950 MHz is refused");
    CHECK(!cc1101_profile_set_freq(&q, 0), "0 Hz is refused");
    CHECK(memcmp(&q, &before, sizeof q) == 0, "a refused frequency leaves the profile unchanged");
}

static void probe_and_faults(void)
{
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    uint8_t part = 0xAA, ver = 0;

    fresh(&sim, &bus);
    CHECK(cc1101_probe(&bus, &part, &ver) == CC1101_OK, "healthy chip probes");
    CHECK(part == 0x00 && ver == 0x14, "PARTNUM/VERSION reported");

    fresh(&sim, &bus);
    sim.dead_bus = true;   /* SO low: CHIP_RDYn looks fine, every byte is 0x00 */
    CHECK(cc1101_probe(&bus, NULL, NULL) == CC1101_ERR_NO_CHIP, "all-zero bus is not mistaken for a chip");

    fresh(&sim, &bus);
    sim.never_ready = true;
    uint8_t v = 0;
    CHECK(cc1101_read_reg(&bus, CC1101_FREQ2, &v) == CC1101_ERR_NOT_READY, "SO stuck high faults");
    CHECK(sim.now_us <= CC1101_READY_TIMEOUT_US + 2, "SO-high wait is bounded");
    CHECK(!sim.cs, "CSn released after a ready timeout");

    fresh(&sim, &bus);
    sim.xfer_fails = true;
    CHECK(cc1101_read_reg(&bus, CC1101_FREQ2, &v) == CC1101_ERR_BUS, "transport failure surfaces");
    CHECK(!sim.cs, "CSn released after a transport failure");

    fresh(&sim, &bus);
    sim.marc_flicker = 2;
    uint8_t marc = 0;
    CHECK(cc1101_read_status(&bus, CC1101_MARCSTATE, &marc) == CC1101_OK && marc == CC1101_MARC_IDLE,
          "status read settles on two agreeing reads");
    sim.marc_flicker = 50;
    CHECK(cc1101_read_status(&bus, CC1101_MARCSTATE, &marc) == CC1101_ERR_UNSTABLE,
          "status read that never agrees is reported, bounded");
    CHECK(cc1101_read_status(&bus, 0x2E, &marc) == CC1101_ERR_ARG, "status read rejects config addresses");
    CHECK(cc1101_read_reg(&bus, 0x30, &v) == CC1101_ERR_ARG, "config read rejects status addresses");
}

static void receive_only_surface(void)
{
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    fresh(&sim, &bus);

    int allowed = 0;
    for (int s = 0; s <= 0xFF; s++) {
        const int txns = sim.txns;
        const cc1101_err_t err = cc1101_strobe(&bus, (uint8_t)s, NULL);
        if (err == CC1101_OK) allowed++;
        else CHECK(sim.txns == txns, "a refused strobe never reaches the bus");
    }
    CHECK(allowed == 7, "exactly seven receive-side strobes are sendable");
    fresh(&sim, &bus);
    CHECK(cc1101_strobe(&bus, 0x31, NULL) == CC1101_ERR_ARG, "SFSTXON refused");
    CHECK(cc1101_strobe(&bus, 0x35, NULL) == CC1101_ERR_ARG, "STX refused");
    CHECK(cc1101_strobe(&bus, 0x3B, NULL) == CC1101_ERR_ARG, "SFTX refused");
    CHECK(sim.strobes == 0, "no refused strobe was executed");

    static const uint8_t blocked[] = { 0x29, 0x2A, 0x2B, 0x2F, 0x30, 0x35, 0x3E, 0x3F, 0x7E, 0xFF };
    for (size_t i = 0; i < sizeof blocked; i++)
        CHECK(cc1101_write_reg(&bus, blocked[i], 0xC0) == CC1101_ERR_ARG, "write outside 0x00..0x2E refused");
    CHECK(sim.forbidden == 0 && sim.txns == 0, "no PATABLE/FIFO/test write reached the bus");
}

static void reset_paths(void)
{
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    bool edge = false;

    fresh(&sim, &bus);
    sim.reg[CC1101_FREQ2] = 0x10;
    sim.marc = CC1101_MARC_RX;
    CHECK(cc1101_reset(&bus, &edge) == CC1101_OK, "reset succeeds");
    CHECK(edge, "SO edge seen on a well-behaved chip");
    CHECK(sim.reg[CC1101_FREQ2] == CC1101_POR_FREQ2 && sim.marc == CC1101_MARC_IDLE, "chip back at POR/IDLE");

    fresh(&sim, &bus);
    sim.so_high_after_sres = true;   /* the Koko: SO never falls after SRES */
    sim.reg[CC1101_FREQ2] = 0x10;
    CHECK(cc1101_reset(&bus, &edge) == CC1101_OK, "reset verified without the SO edge");
    CHECK(!edge, "missing SO edge is reported");
    CHECK(sim.now_us < 10000u, "missing SO edge costs a bounded wait, not a hang");

    fresh(&sim, &bus);
    sim.sres_ignored = true;
    sim.reg[CC1101_FREQ2] = 0x10;
    CHECK(cc1101_reset(&bus, NULL) == CC1101_ERR_RESET, "a reset that did not happen is caught");

    fresh(&sim, &bus);
    sim.never_ready = true;
    CHECK(cc1101_reset(&bus, NULL) == CC1101_ERR_NOT_READY, "reset faults when the chip never readies");
    CHECK(!sim.cs, "CSn released after a failed reset");
}

static void apply_paths(void)
{
    const cc1101_profile_t *p = &cc1101_profile_ook_433_async;
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    cc1101_apply_report_t r;

    fresh(&sim, &bus);
    sim.marc = CC1101_MARC_RX;
    CHECK(cc1101_apply_profile(&bus, p, &r) == CC1101_OK, "profile applies");
    CHECK(r.written == WRITABLE_COUNT && r.failed == 0 && r.retried == 0 && r.first_failed == -1,
          "every writable register written once and verified");
    CHECK(sim.strobes >= 1 && sim.strobe[0] == CC1101_SIDLE, "SIDLE issued before any write");
    CHECK(sim.write_addr[0] == CC1101_IOCFG0 && sim.write_addr[1] == CC1101_IOCFG2,
          "GDO0 then GDO2 written first");
    CHECK(sim.forbidden == 0, "no test register written");
    int same = 1;
    for (uint8_t a = 0; a < CC1101_CFG_COUNT; a++)
        if (cc1101_profile_writable(a) && sim.reg[a] != p->reg[a]) same = 0;
    CHECK(same, "chip registers match the profile");

    fresh(&sim, &bus);
    sim.drop_write_every = 5;
    CHECK(cc1101_apply_profile(&bus, p, &r) == CC1101_OK, "dropped writes recovered by retry");
    CHECK(r.retried > 0 && r.failed == 0 && r.written == WRITABLE_COUNT, "retries are counted");

    fresh(&sim, &bus);
    sim.ignore_writes = true;
    CHECK(cc1101_apply_profile(&bus, p, &r) == CC1101_ERR_VERIFY, "ignored writes are a verify error");
    CHECK(r.failed > 0 && r.first_failed == CC1101_IOCFG0, "first failure named");

    fresh(&sim, &bus);
    sim.wedge_after_sres = true;     /* Koko: reads fine after SRES, writes lost */
    CHECK(cc1101_reset(&bus, NULL) == CC1101_OK, "wedging reset still looks successful");
    CHECK(cc1101_apply_profile(&bus, p, &r) == CC1101_ERR_VERIFY, "wedged chip caught at apply");

    fresh(&sim, &bus);
    sim.never_ready = true;
    CHECK(cc1101_apply_profile(&bus, p, &r) == CC1101_ERR_NOT_READY, "apply aborts on a chip that is not ready");
    CHECK(sim.now_us < 20000u, "and does not retry its way through every register");

    fresh(&sim, &bus);
    sim.not_ready_after_txns = 12;   /* brown-out partway through the writes */
    CHECK(cc1101_apply_profile(&bus, p, &r) == CC1101_ERR_NOT_READY, "mid-apply loss of chip aborts");
    CHECK(sim.now_us < 3u * CC1101_READY_TIMEOUT_US, "abort is immediate, not per-register retries");

    fresh(&sim, &bus);
    sim.dead_bus = true;
    CHECK(cc1101_apply_profile(&bus, p, &r) != CC1101_OK, "dead bus never reports a clean apply");
}

static void verify_paths(void)
{
    const cc1101_profile_t *p = &cc1101_profile_ook_433_async;
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    int16_t at = 0;

    fresh(&sim, &bus);
    CHECK(cc1101_apply_profile(&bus, p, NULL) == CC1101_OK, "apply before verify");
    CHECK(cc1101_strobe(&bus, CC1101_SRX, NULL) == CC1101_OK && sim.calibrated, "SRX autocalibrates");
    CHECK(sim.reg[0x25] != p->reg[0x25], "calibration changed FSCAL1");
    CHECK(cc1101_verify_profile(&bus, p, &at) == CC1101_OK && at == -1,
          "verify ignores calibration results");

    cc1101_sim_t keep = sim;
    cc1101_sim_init(&sim);                       /* brown-out: silent POR */
    sim.now_us = keep.now_us;
    CHECK(cc1101_verify_profile(&bus, p, &at) == CC1101_ERR_VERIFY, "silent reset detected");
    CHECK(at == CC1101_IOCFG2, "first drifted register named");

    CHECK(cc1101_verify_mask(0x23) == 0xF0 && cc1101_verify_mask(0x24) == 0x20 &&
          cc1101_verify_mask(0x25) == 0x00 && cc1101_verify_mask(0x12) == 0xFF,
          "verify masks cover exactly the FSCAL result fields");
}

int main(void)
{
    profile_data();
    probe_and_faults();
    receive_only_surface();
    reset_paths();
    apply_paths();
    verify_paths();
    printf("cc1101_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
