/*
 * cc1101_sim.c — see cc1101_sim.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_sim.h"

#include <string.h>

#define FSTX_PATABLE 0x3E
#define FIFO 0x3F

void cc1101_sim_init(cc1101_sim_t *sim)
{
    memset(sim, 0, sizeof *sim);
    sim->reg[CC1101_IOCFG2] = 0x29;
    sim->reg[CC1101_IOCFG1] = 0x2E;
    sim->reg[CC1101_IOCFG0] = 0x3F;
    sim->reg[CC1101_FREQ2] = CC1101_POR_FREQ2;
    sim->reg[CC1101_FREQ1] = 0xC4;
    sim->reg[CC1101_FREQ0] = 0xEC;
    sim->reg[CC1101_MCSM0] = 0x04;
    sim->reg[0x25] = 0x20;
    sim->marc = CC1101_MARC_IDLE;
}

static uint8_t status_byte(const cc1101_sim_t *sim)
{
    return sim->marc == CC1101_MARC_RX ? 0x10 : 0x00;
}

static uint8_t read_status_reg(cc1101_sim_t *sim, uint8_t addr)
{
    switch (addr) {
    case CC1101_PARTNUM: return 0x00;
    case CC1101_VERSION: return 0x14;
    case CC1101_RSSI: return sim->rssi_moving ? sim->rssi_raw++ : sim->rssi_raw;
    case CC1101_MARCSTATE:
        if (sim->marc_flicker) return (uint8_t)(0x02 + sim->marc_flicker--);
        return sim->marc;
    default: return 0x00;
    }
}

/* Calibration rewrites the FSCAL result fields (SWRS061I §29.1). */
static void calibrate(cc1101_sim_t *sim)
{
    sim->calibrated = true;
    sim->reg[0x23] = (uint8_t)((sim->reg[0x23] & 0xF0) | ((sim->reg[0x23] + 3) & 0x0F));
    sim->reg[0x24] = (uint8_t)((sim->reg[0x24] & 0x20) | ((sim->reg[0x24] + 5) & 0x1F));
    sim->reg[0x25] = (uint8_t)((sim->reg[0x25] + 7) & 0x3F);
}

static void do_strobe(cc1101_sim_t *sim, uint8_t s)
{
    if (sim->strobes < SIM_LOG) sim->strobe[sim->strobes] = s;
    sim->strobes++;
    switch (s) {
    case CC1101_SRES:
        if (sim->sres_ignored) break;
        {
            cc1101_sim_t keep = *sim;
            cc1101_sim_init(sim);
            /* Faults, clock, logs and bus state survive a chip reset. */
            memcpy(sim->write_addr, keep.write_addr, sizeof sim->write_addr);
            sim->writes = keep.writes; sim->strobes = keep.strobes;
            memcpy(sim->strobe, keep.strobe, sizeof sim->strobe);
            sim->forbidden = keep.forbidden; sim->txns = keep.txns; sim->now_us = keep.now_us;
            sim->so_high_after_sres = keep.so_high_after_sres;
            sim->wedge_after_sres = keep.wedge_after_sres;
            sim->drop_write_every = keep.drop_write_every; sim->write_seq = keep.write_seq;
            sim->ignore_writes = keep.ignore_writes || keep.wedge_after_sres;
            sim->cs = keep.cs; sim->byte_in_txn = keep.byte_in_txn; sim->hdr = keep.hdr;
            sim->so_high_latched = keep.so_high_after_sres;
        }
        break;
    case CC1101_SIDLE: sim->marc = CC1101_MARC_IDLE; break;
    case CC1101_SCAL: calibrate(sim); sim->marc = CC1101_MARC_IDLE; break;
    case CC1101_SRX:
        if (sim->srx_ignored) break;
        if (sim->marc == CC1101_MARC_IDLE && ((sim->reg[CC1101_MCSM0] >> 4) & 0x3) == 1)
            calibrate(sim);
        sim->marc = CC1101_MARC_RX;
        break;
    default: break;
    }
}

static void do_write(cc1101_sim_t *sim, uint8_t addr, uint8_t value)
{
    if ((addr >= CC1101_FSTEST && addr <= CC1101_AGCTEST) || addr >= FSTX_PATABLE) {
        sim->forbidden++;
        return;
    }
    if (sim->ignore_writes) return;
    sim->write_seq++;
    if (sim->drop_write_every && sim->write_seq % sim->drop_write_every == 0) return;
    sim->reg[addr] = value;
    if (sim->writes < SIM_LOG) sim->write_addr[sim->writes] = addr;
    sim->writes++;
}

static uint8_t clock_byte(cc1101_sim_t *sim, uint8_t in)
{
    if (sim->dead_bus) return 0x00;
    const int pos = sim->byte_in_txn++;
    if (pos == 0) {
        sim->hdr = in;
        const uint8_t addr = in & 0x3F;
        const bool burst = in & 0x40;
        const uint8_t status = status_byte(sim);
        if (addr >= 0x30 && addr <= 0x3D && !burst) do_strobe(sim, addr);
        return status;
    }
    const uint8_t addr = sim->hdr & 0x3F;
    const bool read = sim->hdr & 0x80;
    const bool burst = sim->hdr & 0x40;
    if (read) {
        if (addr < CC1101_CFG_COUNT) return sim->reg[addr];
        if (addr >= 0x30 && addr <= 0x3D && burst) return read_status_reg(sim, addr);
        return 0x00;
    }
    if (pos == 1 && (addr < 0x30 || addr >= FSTX_PATABLE)) do_write(sim, addr, in);
    return status_byte(sim);
}

static bool sim_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    cc1101_sim_t *sim = ctx;
    if (sim->xfer_fails) return false;
    for (size_t i = 0; i < len; i++) rx[i] = clock_byte(sim, tx[i]);
    return true;
}

static void sim_cs(void *ctx, bool asserted)
{
    cc1101_sim_t *sim = ctx;
    if (asserted && !sim->cs) {
        sim->byte_in_txn = 0;
        sim->txns++;
        if (sim->not_ready_after_txns && sim->txns >= sim->not_ready_after_txns) sim->never_ready = true;
    }
    if (!asserted) sim->so_high_latched = false;
    sim->cs = asserted;
}

static int sim_so(void *ctx)
{
    cc1101_sim_t *sim = ctx;
    if (sim->never_ready) return 1;
    if (sim->dead_bus) return 0;
    if (!sim->cs) return 1;
    return sim->so_high_latched ? 1 : 0;
}

static void sim_delay(void *ctx, uint32_t us) { ((cc1101_sim_t *)ctx)->now_us += us; }
static uint32_t sim_now(void *ctx) { return ((cc1101_sim_t *)ctx)->now_us; }

cc1101_bus_t cc1101_sim_bus(cc1101_sim_t *sim)
{
    const cc1101_bus_t bus = { sim, sim_xfer, sim_cs, sim_so, sim_delay, sim_now };
    return bus;
}
