/*
 * cc1101_config.h — apply a cc1101_profile_t with read-back verification
 * (DESIGN §6.1). A write that does not read back is a reported fault,
 * never a silent misconfiguration.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_CONFIG_H
#define OSCILLA_CC1101_CONFIG_H

#include <stdint.h>

#include "cc1101_bus.h"
#include "cc1101_profile.h"

#define CC1101_WRITE_ATTEMPTS 3

typedef struct {
    uint8_t written;       /* registers that read back correctly */
    uint8_t retried;       /* of those, how many needed more than one attempt */
    uint8_t failed;        /* registers that never read back */
    int16_t first_failed;  /* address of the first failure, or -1 */
} cc1101_apply_report_t;

/* SIDLE, then write every writable register. GDO0 and GDO2 go first because
 * the POR GDO0 clock (SWRS061I §19.1) can disturb the bus. Returns
 * CC1101_ERR_VERIFY if any register failed; report (may be NULL) says which. */
cc1101_err_t cc1101_apply_profile(const cc1101_bus_t *bus, const cc1101_profile_t *profile,
                                  cc1101_apply_report_t *report);

/* Compare live registers with the profile, ignoring calibration-result bits.
 * Detects a silent reset or a brown-out. *first_mismatch is -1 when clean. */
cc1101_err_t cc1101_verify_profile(const cc1101_bus_t *bus, const cc1101_profile_t *profile,
                                   int16_t *first_mismatch);

/* Bits of addr that verify compares (0xFF except the FSCAL result fields). */
uint8_t cc1101_verify_mask(uint8_t addr);

#endif /* OSCILLA_CC1101_CONFIG_H */
