/*
 * radio_arbiter.h — single owner of the C5's shared PHY (DESIGN §6.2).
 *
 * Wi-Fi, BLE and 802.15.4 share one radio: exactly one may own it. An owner
 * registers a teardown hook so `stop` can force it down. The LoRa lane is a
 * separate chip and arrives in P3; the power interlock stays conservative
 * until P6 measures real current.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_RADIO_ARBITER_H
#define OSCILLA_RADIO_ARBITER_H

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    PHY_OWNER_NONE = 0,
    PHY_OWNER_WIFI,
    PHY_OWNER_BLE,
    PHY_OWNER_IEEE802154,
} phy_owner_t;

/* Must return only once the owner has released the PHY (or given up). */
typedef void (*arbiter_teardown_fn)(void);

esp_err_t arbiter_init(void);

/* ESP_OK, or ESP_ERR_INVALID_STATE if another owner holds the PHY. */
esp_err_t arbiter_acquire(phy_owner_t who, arbiter_teardown_fn teardown);

void arbiter_release(phy_owner_t who);

phy_owner_t arbiter_owner(void);
const char *arbiter_owner_name(phy_owner_t who);   /* OCP_OWNER_* */

/* Force the current owner down. Returns true if something was running. */
bool arbiter_stop_all(void);

#endif /* OSCILLA_RADIO_ARBITER_H */
