/*
 * cc1101_bus.c — see cc1101_bus.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_bus.h"

#include <string.h>

#define HDR_READ  0x80
#define HDR_BURST 0x40
#define STATUS_CHIP_RDYN 0x80

#define STATUS_READ_TRIES   4
#define RESET_SO_TIMEOUT_US 1000u
#define RESET_SETTLE_US     1000u
#define RESET_IDLE_TIMEOUT_US 5000u
#define STATE_POLL_US       10u

const char *cc1101_err_name(cc1101_err_t err)
{
    switch (err) {
    case CC1101_OK: return "ok";
    case CC1101_ERR_ARG: return "arg";
    case CC1101_ERR_BUS: return "bus";
    case CC1101_ERR_NOT_READY: return "not_ready";
    case CC1101_ERR_NO_CHIP: return "no_chip";
    case CC1101_ERR_UNSTABLE: return "unstable";
    case CC1101_ERR_RESET: return "reset";
    case CC1101_ERR_VERIFY: return "verify";
    case CC1101_ERR_STATE: return "state";
    }
    return "unknown";
}

static bool elapsed(const cc1101_bus_t *bus, uint32_t start, uint32_t timeout_us)
{
    return (uint32_t)(bus->now_us(bus->ctx) - start) >= timeout_us;
}

/* With CSn already low: wait for SO low (CHIP_RDYn), bounded. */
static bool wait_so_low(const cc1101_bus_t *bus, uint32_t timeout_us)
{
    const uint32_t start = bus->now_us(bus->ctx);
    while (bus->so(bus->ctx)) {
        if (elapsed(bus, start, timeout_us)) return false;
        bus->delay_us(bus->ctx, 1);
    }
    return true;
}

static cc1101_err_t transaction(const cc1101_bus_t *bus, const uint8_t *tx, uint8_t *rx, size_t len)
{
    bus->cs(bus->ctx, true);
    if (!wait_so_low(bus, CC1101_READY_TIMEOUT_US)) {
        bus->cs(bus->ctx, false);
        return CC1101_ERR_NOT_READY;
    }
    const bool ok = bus->xfer(bus->ctx, tx, rx, len);
    bus->cs(bus->ctx, false);
    if (!ok) return CC1101_ERR_BUS;
    return (rx[0] & STATUS_CHIP_RDYN) ? CC1101_ERR_NOT_READY : CC1101_OK;
}

cc1101_err_t cc1101_read_reg(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out)
{
    if (!bus || !out || addr >= CC1101_CFG_COUNT) return CC1101_ERR_ARG;
    const uint8_t tx[2] = { (uint8_t)(HDR_READ | addr), 0 };
    uint8_t rx[2] = { 0 };
    const cc1101_err_t err = transaction(bus, tx, rx, sizeof tx);
    if (err == CC1101_OK) *out = rx[1];
    return err;
}

cc1101_err_t cc1101_write_reg(const cc1101_bus_t *bus, uint8_t addr, uint8_t value)
{
    if (!bus || addr >= CC1101_CFG_COUNT) return CC1101_ERR_ARG;
    if (addr >= CC1101_FSTEST && addr <= CC1101_AGCTEST) return CC1101_ERR_ARG;
    const uint8_t tx[2] = { addr, value };
    uint8_t rx[2] = { 0 };
    return transaction(bus, tx, rx, sizeof tx);
}

static cc1101_err_t read_status_once(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out)
{
    const uint8_t tx[2] = { (uint8_t)(HDR_READ | HDR_BURST | addr), 0 };
    uint8_t rx[2] = { 0 };
    const cc1101_err_t err = transaction(bus, tx, rx, sizeof tx);
    if (err == CC1101_OK) *out = rx[1];
    return err;
}

cc1101_err_t cc1101_read_status(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out)
{
    if (!bus || !out || addr < CC1101_PARTNUM || addr > 0x3D) return CC1101_ERR_ARG;
    uint8_t prev = 0;
    cc1101_err_t err = read_status_once(bus, addr, &prev);
    for (int i = 1; err == CC1101_OK && i < STATUS_READ_TRIES; i++) {
        uint8_t cur = 0;
        err = read_status_once(bus, addr, &cur);
        if (err == CC1101_OK && cur == prev) {
            *out = cur;
            return CC1101_OK;
        }
        prev = cur;
    }
    return err == CC1101_OK ? CC1101_ERR_UNSTABLE : err;
}

cc1101_err_t cc1101_read_status_raw(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out)
{
    if (!bus || !out || addr < CC1101_PARTNUM || addr > 0x3D) return CC1101_ERR_ARG;
    return read_status_once(bus, addr, out);
}

cc1101_err_t cc1101_read_rxfifo(const cc1101_bus_t *bus, uint8_t *out, size_t n)
{
    if (!bus || !out || n == 0 || n > CC1101_FIFO_SIZE) return CC1101_ERR_ARG;
    uint8_t tx[1 + CC1101_FIFO_SIZE] = { HDR_READ | HDR_BURST | 0x3F };
    uint8_t rx[1 + CC1101_FIFO_SIZE] = { 0 };
    const cc1101_err_t err = transaction(bus, tx, rx, 1 + n);
    if (err == CC1101_OK) memcpy(out, rx + 1, n);
    return err;
}

static bool strobe_allowed(uint8_t s)
{
    switch (s) {
    case CC1101_SRES: case CC1101_SCAL: case CC1101_SRX: case CC1101_SIDLE:
    case CC1101_SPWD: case CC1101_SFRX: case CC1101_SNOP:
        return true;
    default:
        return false;
    }
}

cc1101_err_t cc1101_strobe(const cc1101_bus_t *bus, uint8_t strobe, uint8_t *status)
{
    if (!bus || !strobe_allowed(strobe)) return CC1101_ERR_ARG;
    const uint8_t tx[1] = { strobe };
    uint8_t rx[1] = { 0 };
    const cc1101_err_t err = transaction(bus, tx, rx, sizeof tx);
    if (status) *status = rx[0];
    return err;
}

cc1101_err_t cc1101_probe(const cc1101_bus_t *bus, uint8_t *partnum, uint8_t *version)
{
    if (!bus) return CC1101_ERR_ARG;
    uint8_t part = 0, ver = 0, marc = 0;
    cc1101_err_t err = cc1101_read_status(bus, CC1101_PARTNUM, &part);
    if (err == CC1101_OK) err = cc1101_read_status(bus, CC1101_VERSION, &ver);
    if (err == CC1101_OK) err = cc1101_read_status(bus, CC1101_MARCSTATE, &marc);
    if (err != CC1101_OK) return err;
    if (partnum) *partnum = part;
    if (version) *version = ver;
    /* SO held low reads as a ready chip returning zeros; SLEEP is unreadable. */
    if (ver == 0x00 || ver == 0xFF || (marc & 0x1F) == 0x00) return CC1101_ERR_NO_CHIP;
    return CC1101_OK;
}

cc1101_err_t cc1101_wait_state(const cc1101_bus_t *bus, uint8_t want, uint32_t timeout_us,
                               uint8_t *last)
{
    if (!bus) return CC1101_ERR_ARG;
    const uint32_t start = bus->now_us(bus->ctx);
    for (;;) {
        uint8_t marc = 0;
        const cc1101_err_t err = cc1101_read_status(bus, CC1101_MARCSTATE, &marc);
        if (err == CC1101_OK) {
            marc &= 0x1F;
            if (last) *last = marc;
            if (marc == want) return CC1101_OK;
        } else if (err != CC1101_ERR_UNSTABLE) {
            return err;
        }
        if (elapsed(bus, start, timeout_us)) return CC1101_ERR_STATE;
        bus->delay_us(bus->ctx, STATE_POLL_US);
    }
}

cc1101_err_t cc1101_reset(const cc1101_bus_t *bus, bool *so_edge)
{
    if (!bus) return CC1101_ERR_ARG;
    if (so_edge) *so_edge = false;

    bus->cs(bus->ctx, true);
    bus->delay_us(bus->ctx, 5);
    bus->cs(bus->ctx, false);
    bus->delay_us(bus->ctx, 40);
    bus->cs(bus->ctx, true);
    if (!wait_so_low(bus, CC1101_READY_TIMEOUT_US)) {
        bus->cs(bus->ctx, false);
        return CC1101_ERR_NOT_READY;
    }
    const uint8_t tx[1] = { CC1101_SRES };
    uint8_t rx[1] = { 0 };
    const bool ok = bus->xfer(bus->ctx, tx, rx, sizeof tx);
    /* Some modules never drop SO here (WORKLOG 2026-11-23); bounded, not required. */
    const bool edge = ok && wait_so_low(bus, RESET_SO_TIMEOUT_US);
    bus->cs(bus->ctx, false);
    if (!ok) return CC1101_ERR_BUS;
    if (so_edge) *so_edge = edge;
    bus->delay_us(bus->ctx, RESET_SETTLE_US);

    uint8_t freq2 = 0;
    cc1101_err_t err = cc1101_read_reg(bus, CC1101_FREQ2, &freq2);
    if (err != CC1101_OK) return err;
    if (freq2 != CC1101_POR_FREQ2) return CC1101_ERR_RESET;
    err = cc1101_wait_state(bus, CC1101_MARC_IDLE, RESET_IDLE_TIMEOUT_US, NULL);
    return err == CC1101_ERR_STATE ? CC1101_ERR_RESET : err;
}
