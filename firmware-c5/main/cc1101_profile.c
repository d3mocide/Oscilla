/*
 * cc1101_profile.c — see cc1101_profile.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_profile.h"

/* Register values read back from a CC1101 that received SDR-confirmed and
 * bench test transmissions (WORKLOG 2026-11-23). Values only; no driver code. */
const cc1101_profile_t cc1101_profile_ook_433_async = {
    .name = "ook-433-async",
    .fxosc_hz = 26000000u,
    .reg = {
        0x0D, 0x2E, 0x0D, 0x07, 0xD3, 0x91, 0x00, 0x04,  /* 0x00 */
        0x32, 0x00, 0x00, 0x0F, 0x23, 0x10, 0xB0, 0x71,  /* 0x08 */
        0x17, 0x93, 0x32, 0x02, 0xF8, 0x47, 0x07, 0x30,  /* 0x10 */
        0x18, 0x16, 0x1C, 0xC7, 0x00, 0xB2, 0x87, 0x6B,  /* 0x18 */
        0xF8, 0x56, 0x11, 0xEF, 0x2B, 0x17, 0x1F, 0x41,  /* 0x20 */
        0x00, 0x59, 0x7F, 0x3C, 0x81, 0x35, 0x09,  /* 0x28 */
    },
};

bool cc1101_profile_writable(uint8_t addr)
{
    return addr < CC1101_CFG_COUNT && (addr < CC1101_FSTEST || addr > CC1101_AGCTEST);
}

uint32_t cc1101_profile_freq_hz(const cc1101_profile_t *p)
{
    const uint64_t word = ((uint64_t)p->reg[CC1101_FREQ2] << 16) |
                          ((uint64_t)p->reg[CC1101_FREQ1] << 8) | p->reg[CC1101_FREQ0];
    return (uint32_t)((word * p->fxosc_hz) >> 16);
}

/* SWRS061I §1: 300-348, 387-464 and 779-928 MHz. */
static bool in_band(uint32_t hz)
{
    return (hz >= 300000000u && hz <= 348000000u) || (hz >= 387000000u && hz <= 464000000u) ||
           (hz >= 779000000u && hz <= 928000000u);
}

#define MDMCFG4 0x10
#define MDMCFG3 0x11
#define MDMCFG2 0x12
#define PKTCTRL0 0x08
#define GDO_HIGH_Z 0x2E

uint32_t cc1101_profile_set_drate(cc1101_profile_t *p, uint32_t baud)
{
    if (!p || p->fxosc_hz == 0 || baud == 0) return 0;
    uint64_t best_err = UINT64_MAX, best_rate = 0;
    uint8_t best_e = 0, best_m = 0;
    for (uint8_t e = 0; e <= 15; e++) {
        for (uint16_t m = 0; m <= 255; m++) {
            /* R = (256 + M) * 2^E * fXOSC / 2^28 */
            const uint64_t rate = (((uint64_t)(256u + m) << e) * p->fxosc_hz) >> 28;
            const uint64_t err = rate > baud ? rate - baud : baud - rate;
            if (err < best_err) {
                best_err = err;
                best_rate = rate;
                best_e = e;
                best_m = (uint8_t)m;
            }
        }
    }
    if (best_rate == 0 || best_err * 50 > baud) return 0;   /* > 2 % off: not expressible */
    p->reg[MDMCFG4] = (uint8_t)((p->reg[MDMCFG4] & 0xF0) | best_e);
    p->reg[MDMCFG3] = best_m;
    return (uint32_t)best_rate;
}

uint32_t cc1101_profile_make_fifo(cc1101_profile_t *p, uint32_t baud)
{
    if (!p) return 0;
    cc1101_profile_t q = *p;
    const uint32_t achieved = cc1101_profile_set_drate(&q, baud);
    if (achieved == 0) return 0;
    q.reg[PKTCTRL0] = 0x02;   /* whitening off, normal FIFO, CRC off, infinite */
    q.reg[MDMCFG2] = (uint8_t)((q.reg[MDMCFG2] & 0x80) | 0x30);   /* OOK, no Manchester, no sync */
    q.reg[CC1101_IOCFG0] = GDO_HIGH_Z;
    q.reg[CC1101_IOCFG2] = GDO_HIGH_Z;
    *p = q;
    return achieved;
}

bool cc1101_profile_set_freq(cc1101_profile_t *p, uint32_t hz)
{
    if (!p || p->fxosc_hz == 0 || !in_band(hz)) return false;
    /* FREQ = f * 2^16 / fXOSC (SWRS061I §21), rounded to nearest. */
    const uint64_t word = (((uint64_t)hz << 16) + p->fxosc_hz / 2) / p->fxosc_hz;
    if (word > 0x3FFFFFu) return false;
    p->reg[CC1101_FREQ2] = (uint8_t)(word >> 16);
    p->reg[CC1101_FREQ1] = (uint8_t)(word >> 8);
    p->reg[CC1101_FREQ0] = (uint8_t)word;
    return true;
}
