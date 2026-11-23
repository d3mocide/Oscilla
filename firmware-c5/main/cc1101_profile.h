/*
 * cc1101_profile.h — CC1101 receive register profiles as plain data
 * (DESIGN §6.1). Applied and verified by cc1101_config.c.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_PROFILE_H
#define OSCILLA_CC1101_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "cc1101_bus.h"

typedef struct {
    const char *name;
    uint32_t fxosc_hz;
    uint8_t reg[CC1101_CFG_COUNT];   /* full 0x00..0x2E image */
} cc1101_profile_t;

/* 433.92 MHz OOK, async serial data on GDO0/GDO2, 650 kHz RX bandwidth,
 * MCSM0.FS_AUTOCAL=01 (calibrate on IDLE->RX), 26 MHz crystal. */
extern const cc1101_profile_t cc1101_profile_ook_433_async;

/* False for FSTEST, PTEST and AGCTEST, which a profile never writes. */
bool cc1101_profile_writable(uint8_t addr);

/* Carrier frequency encoded in FREQ2..FREQ0 (CHANNR not applied). */
uint32_t cc1101_profile_freq_hz(const cc1101_profile_t *p);

/* Rewrite FREQ2..FREQ0 for hz; false (unchanged) outside the CC1101 bands. */
bool cc1101_profile_set_freq(cc1101_profile_t *p, uint32_t hz);

/* Set MDMCFG4[3:0]/MDMCFG3 to the closest data rate to baud (SWRS061I
 * §29.1 MDMCFG3). Returns the achieved rate in baud, or 0 (unchanged) if
 * baud is outside what the registers can express. */
uint32_t cc1101_profile_set_drate(cc1101_profile_t *p, uint32_t baud);

/* Turn an OOK profile into FIFO sampling: normal FIFO mode, infinite length,
 * no sync word, CRC and whitening off, GDO0/GDO2 three-stated, sampled at
 * baud. Pins stay as they are on the C5 (no GDO wire needed). Returns the
 * achieved rate, or 0 (unchanged) on a bad rate. */
uint32_t cc1101_profile_make_fifo(cc1101_profile_t *p, uint32_t baud);

#endif /* OSCILLA_CC1101_PROFILE_H */
