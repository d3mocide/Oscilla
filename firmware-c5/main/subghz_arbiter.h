/*
 * subghz_arbiter.h — single owner of the shared Wio/CC1101 SPI bus
 * (c5-dual-radio-wiring.md §5.2).
 *
 * Structural mirror of radio_arbiter.h: exactly one of the two sub-GHz
 * receive engines may run at a time, independent of the PHY lane. A
 * transition to one radio first stops and deselects the other.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_SUBGHZ_ARBITER_H
#define OSCILLA_SUBGHZ_ARBITER_H

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    SUBGHZ_OWNER_NONE = 0,
    SUBGHZ_OWNER_LORA,
    SUBGHZ_OWNER_CC1101,
} subghz_owner_t;

/* Must return only once the owner has released the bus (or given up). */
typedef void (*subghz_teardown_fn)(void);

esp_err_t subghz_arbiter_init(void);

/* ESP_OK, or ESP_ERR_INVALID_STATE if the other radio holds the bus. */
esp_err_t subghz_arbiter_acquire(subghz_owner_t who, subghz_teardown_fn teardown);

void subghz_arbiter_release(subghz_owner_t who);

subghz_owner_t subghz_arbiter_owner(void);

/* Force the current owner down. Returns true if something was running. */
bool subghz_arbiter_stop(void);

#endif /* OSCILLA_SUBGHZ_ARBITER_H */
