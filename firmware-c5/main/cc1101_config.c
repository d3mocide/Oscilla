/*
 * cc1101_config.c — see cc1101_config.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_config.h"

#define FSCAL3 0x23
#define FSCAL2 0x24
#define FSCAL1 0x25
#define IDLE_TIMEOUT_US 5000u

uint8_t cc1101_verify_mask(uint8_t addr)
{
    /* Calibration results change on every autocal (SWRS061I §29.1). */
    switch (addr) {
    case FSCAL3: return 0xF0;
    case FSCAL2: return 0x20;
    case FSCAL1: return 0x00;
    default: return 0xFF;
    }
}

/* Returns attempts used (1..CC1101_WRITE_ATTEMPTS), 0 if it never read back,
 * or a negative cc1101_err_t on a transport-level failure. */
static int write_verified(const cc1101_bus_t *bus, uint8_t addr, uint8_t value)
{
    for (int attempt = 1; attempt <= CC1101_WRITE_ATTEMPTS; attempt++) {
        cc1101_err_t err = cc1101_write_reg(bus, addr, value);
        uint8_t back = 0;
        if (err == CC1101_OK) err = cc1101_read_reg(bus, addr, &back);
        if (err == CC1101_ERR_BUS || err == CC1101_ERR_ARG || err == CC1101_ERR_NOT_READY)
            return -(int)err;
        if (err == CC1101_OK && back == value) return attempt;
    }
    return 0;
}

static cc1101_err_t apply_one(const cc1101_bus_t *bus, const cc1101_profile_t *profile,
                              uint8_t addr, cc1101_apply_report_t *r)
{
    const int used = write_verified(bus, addr, profile->reg[addr]);
    if (used < 0) return (cc1101_err_t)(-used);
    if (used == 0) {
        r->failed++;
        if (r->first_failed < 0) r->first_failed = addr;
    } else {
        r->written++;
        if (used > 1) r->retried++;
    }
    return CC1101_OK;
}

cc1101_err_t cc1101_apply_profile(const cc1101_bus_t *bus, const cc1101_profile_t *profile,
                                  cc1101_apply_report_t *report)
{
    cc1101_apply_report_t r = { 0, 0, 0, -1 };
    if (report) *report = r;
    if (!bus || !profile) return CC1101_ERR_ARG;

    /* Configuration is only updated in IDLE (SWRS061I §10.1). */
    cc1101_err_t err = cc1101_strobe(bus, CC1101_SIDLE, NULL);
    if (err == CC1101_OK) err = cc1101_wait_state(bus, CC1101_MARC_IDLE, IDLE_TIMEOUT_US, NULL);
    if (err != CC1101_OK) return err;

    static const uint8_t first[] = { CC1101_IOCFG0, CC1101_IOCFG2 };
    for (size_t i = 0; i < sizeof first && err == CC1101_OK; i++)
        err = apply_one(bus, profile, first[i], &r);
    for (uint8_t addr = 0; addr < CC1101_CFG_COUNT && err == CC1101_OK; addr++) {
        if (addr == CC1101_IOCFG0 || addr == CC1101_IOCFG2) continue;
        if (!cc1101_profile_writable(addr)) continue;
        err = apply_one(bus, profile, addr, &r);
    }
    if (report) *report = r;
    if (err != CC1101_OK) return err;
    return r.failed ? CC1101_ERR_VERIFY : CC1101_OK;
}

cc1101_err_t cc1101_verify_profile(const cc1101_bus_t *bus, const cc1101_profile_t *profile,
                                   int16_t *first_mismatch)
{
    if (first_mismatch) *first_mismatch = -1;
    if (!bus || !profile) return CC1101_ERR_ARG;
    for (uint8_t addr = 0; addr < CC1101_CFG_COUNT; addr++) {
        if (!cc1101_profile_writable(addr)) continue;
        uint8_t live = 0;
        const cc1101_err_t err = cc1101_read_reg(bus, addr, &live);
        if (err != CC1101_OK) return err;
        const uint8_t mask = cc1101_verify_mask(addr);
        if ((live & mask) != (profile->reg[addr] & mask)) {
            if (first_mismatch) *first_mismatch = addr;
            return CC1101_ERR_VERIFY;
        }
    }
    return CC1101_OK;
}
