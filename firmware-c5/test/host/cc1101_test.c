/*
 * cc1101_test.c — adversarial tests for the CC1101 bus, profile and config
 * layers against cc1101_sim. Build with -fsanitize=address,undefined.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_bus.h"
#include "cc1101_config.h"
#include "cc1101_profile.h"
#include "cc1101_rx.h"
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

static int strobe_index(const cc1101_sim_t *sim, uint8_t s, int from)
{
    for (int i = from; i < sim->strobes && i < SIM_LOG; i++)
        if (sim->strobe[i] == s) return i;
    return -1;
}

static void rx_paths(void)
{
    const cc1101_profile_t *p = &cc1101_profile_ook_433_async;
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    bool cal = false;

    fresh(&sim, &bus);
    CHECK(cc1101_apply_profile(&bus, p, NULL) == CC1101_OK, "apply before rx");
    int mark = sim.strobes;
    CHECK(cc1101_rx_start(&bus, &cal) == CC1101_OK, "rx starts");
    CHECK(cal && sim.calibrated && sim.marc == CC1101_MARC_RX, "autocal profile reaches calibrated RX");
    CHECK(strobe_index(&sim, CC1101_SCAL, mark) < 0, "no redundant SCAL when autocal covers IDLE->RX");
    CHECK(strobe_index(&sim, CC1101_SFRX, mark) >= 0 &&
          strobe_index(&sim, CC1101_SFRX, mark) < strobe_index(&sim, CC1101_SRX, mark),
          "RX FIFO flushed before SRX");

    /* September's driver: MCSM0 left at POR (FS_AUTOCAL=0), no SCAL -> deaf. */
    fresh(&sim, &bus);
    cc1101_profile_t q = *p;
    q.reg[CC1101_MCSM0] = 0x04;
    CHECK(cc1101_apply_profile(&bus, &q, NULL) == CC1101_OK, "apply autocal-off profile");
    mark = sim.strobes;
    CHECK(cc1101_rx_start(&bus, &cal) == CC1101_OK && cal, "rx starts with autocal off");
    CHECK(sim.calibrated, "synthesizer calibrated even with autocal off");
    const int scal = strobe_index(&sim, CC1101_SCAL, mark);
    CHECK(scal >= 0 && scal < strobe_index(&sim, CC1101_SRX, mark), "SCAL issued before SRX");

    fresh(&sim, &bus);
    sim.srx_ignored = true;
    CHECK(cc1101_rx_start(&bus, &cal) == CC1101_ERR_STATE && !cal, "RX never reached is an error");
    CHECK(sim.now_us < 4u * CC1101_RX_START_TIMEOUT_US, "RX wait is bounded");

    fresh(&sim, &bus);
    CHECK(cc1101_rx_start(&bus, NULL) == CC1101_OK, "rx start");
    CHECK(cc1101_rx_stop(&bus) == CC1101_OK && sim.marc == CC1101_MARC_IDLE, "rx stop returns to IDLE");

    fresh(&sim, &bus);
    sim.dead_bus = true;
    CHECK(cc1101_rx_start(&bus, &cal) != CC1101_OK && !cal, "dead bus never reports RX");

    /* SWRS061I §17.3 with RSSI_offset 74. */
    CHECK(cc1101_rssi_dbm(0x00, 74) == -74, "rssi 0x00");
    CHECK(cc1101_rssi_dbm(0x2A, 74) == -53, "rssi 0x2A");
    CHECK(cc1101_rssi_dbm(0x7F, 74) == -11, "rssi 0x7F rounds toward -inf");
    CHECK(cc1101_rssi_dbm(0x80, 74) == -138, "rssi 0x80 is the most negative");
    CHECK(cc1101_rssi_dbm(0xC0, 74) == -106, "rssi 0xC0");
    CHECK(cc1101_rssi_dbm(0xFF, 74) == -75, "rssi 0xFF (-0.5 dB) rounds toward -inf");
    fresh(&sim, &bus);
    sim.rssi_raw = 0xC0;
    int16_t dbm = 0;
    CHECK(cc1101_rx_rssi(&bus, &dbm) == CC1101_OK && dbm == -106, "rssi read through the bus");
    sim.rssi_moving = true;
    int ok = 1;
    for (int i = 0; i < 20; i++) ok &= cc1101_rx_rssi(&bus, &dbm) == CC1101_OK;
    CHECK(ok, "RSSI reads succeed while the value changes on every read");
    CHECK(cc1101_read_status_raw(&bus, 0x2E, NULL) == CC1101_ERR_ARG, "raw status read checks its arguments");
}

static void fifo_paths(void)
{
    cc1101_sim_t sim;
    cc1101_bus_t bus;
    cc1101_profile_t p = cc1101_profile_ook_433_async;

    const uint8_t chanbw = (uint8_t)(p.reg[0x10] & 0xF0);
    const uint32_t got = cc1101_profile_make_fifo(&p, 25000);
    CHECK(got > 24750 && got < 25250, "FIFO profile samples at ~25 kbaud");
    CHECK(p.reg[0x08] == 0x02, "normal FIFO, infinite length, CRC and whitening off");
    CHECK((p.reg[0x12] & 0x7F) == 0x30, "OOK, no Manchester, no sync word");
    CHECK(p.reg[CC1101_IOCFG0] == 0x2E && p.reg[CC1101_IOCFG2] == 0x2E, "GDO0/GDO2 three-stated");
    CHECK(p.reg[CC1101_IOCFG1] == 0x2E, "SO still three-stated for the shared bus");
    CHECK((p.reg[0x10] & 0xF0) == chanbw, "channel bandwidth untouched");
    CHECK(p.reg[CC1101_MCSM0] == cc1101_profile_ook_433_async.reg[CC1101_MCSM0], "autocal kept");
    CHECK(cc1101_profile_freq_hz(&p) == cc1101_profile_freq_hz(&cc1101_profile_ook_433_async),
          "frequency kept");

    cc1101_profile_t d = cc1101_profile_ook_433_async;
    CHECK(cc1101_profile_set_drate(&d, 115051) > 115000 && (d.reg[0x10] & 0x0F) == 12 && d.reg[0x11] == 34,
          "115.051 kBaud gives the datasheet reset E=12, M=34");
    const cc1101_profile_t before = d;
    CHECK(cc1101_profile_set_drate(&d, 0) == 0, "0 baud refused");
    CHECK(cc1101_profile_set_drate(&d, 50000000u) == 0, "unreachable rate refused");
    CHECK(cc1101_profile_make_fifo(&d, 0) == 0, "FIFO profile refuses a bad rate");
    CHECK(memcmp(&d, &before, sizeof d) == 0, "refusals leave the profile unchanged");

    fresh(&sim, &bus);
    CHECK(cc1101_apply_profile(&bus, &p, NULL) == CC1101_OK && cc1101_rx_start(&bus, NULL) == CC1101_OK,
          "FIFO profile applies and receives");
    uint8_t in[40], out[CC1101_FIFO_SIZE];
    for (int i = 0; i < 40; i++) in[i] = (uint8_t)(i * 7 + 1);
    size_t n = 99;
    bool ovf = true;
    const int reads0 = sim.fifo_burst_reads;
    CHECK(cc1101_rx_drain(&bus, out, sizeof out, &n, &ovf) == CC1101_OK && n == 0 && !ovf,
          "empty FIFO drains nothing");
    CHECK(sim.fifo_burst_reads == reads0, "and issues no FIFO read");
    cc1101_sim_fifo_push(&sim, in, 1);
    CHECK(cc1101_rx_drain(&bus, out, sizeof out, &n, &ovf) == CC1101_OK && n == 0,
          "a single byte is left in place");
    cc1101_sim_fifo_push(&sim, in + 1, 39);
    CHECK(cc1101_rx_drain(&bus, out, sizeof out, &n, &ovf) == CC1101_OK && n == 39 && !ovf,
          "drain takes all but the last byte");
    CHECK(memcmp(out, in, 39) == 0, "bytes arrive in order");
    CHECK(sim.fifo_n == 1 && sim.fifo_emptied == 0, "FIFO never emptied while receiving");
    cc1101_sim_fifo_push(&sim, in, 20);
    CHECK(cc1101_rx_drain(&bus, out, 8, &n, &ovf) == CC1101_OK && n == 8 && out[0] == in[39],
          "cap limits the read and order continues");

    fresh(&sim, &bus);
    CHECK(cc1101_apply_profile(&bus, &p, NULL) == CC1101_OK && cc1101_rx_start(&bus, NULL) == CC1101_OK,
          "rx before overflow");
    uint8_t flood[70];
    memset(flood, 0xA5, sizeof flood);
    cc1101_sim_fifo_push(&sim, flood, 70);
    CHECK(sim.fifo_overflow, "sim overflowed");
    CHECK(cc1101_rx_drain(&bus, out, sizeof out, &n, &ovf) == CC1101_OK && ovf && n == 0,
          "overflow reported, no stale bytes returned");
    CHECK(!sim.fifo_overflow && sim.fifo_n == 0 && sim.marc == CC1101_MARC_RX && sim.calibrated,
          "overflow recovered: flushed, recalibrated, back in RX");

    CHECK(cc1101_read_rxfifo(&bus, out, 0) == CC1101_ERR_ARG, "zero-length FIFO read refused");
    CHECK(cc1101_read_rxfifo(&bus, out, CC1101_FIFO_SIZE + 1) == CC1101_ERR_ARG, "oversize FIFO read refused");
    CHECK(cc1101_rx_drain(&bus, out, 0, &n, &ovf) == CC1101_ERR_ARG, "zero-cap drain refused");
    CHECK(sim.forbidden == 0, "no TX FIFO or PATABLE access");
}

int main(void)
{
    profile_data();
    probe_and_faults();
    receive_only_surface();
    reset_paths();
    apply_paths();
    verify_paths();
    rx_paths();
    fifo_paths();
    printf("cc1101_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
