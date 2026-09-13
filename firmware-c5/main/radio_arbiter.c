/*
 * radio_arbiter.c — see radio_arbiter.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "radio_arbiter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ocp.h"

static SemaphoreHandle_t s_lock;
static phy_owner_t s_owner = PHY_OWNER_NONE;
static arbiter_teardown_fn s_teardown;

esp_err_t arbiter_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t arbiter_acquire(phy_owner_t who, arbiter_teardown_fn teardown)
{
    esp_err_t err = ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_owner == PHY_OWNER_NONE) {
        s_owner = who;
        s_teardown = teardown;
        err = ESP_OK;
    }
    xSemaphoreGive(s_lock);
    return err;
}

void arbiter_release(phy_owner_t who)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_owner == who) {
        s_owner = PHY_OWNER_NONE;
        s_teardown = NULL;
    }
    xSemaphoreGive(s_lock);
}

phy_owner_t arbiter_owner(void)
{
    return s_owner;   /* one enum read; a stale value only affects [STATUS] */
}

const char *arbiter_owner_name(phy_owner_t who)
{
    switch (who) {
        case PHY_OWNER_WIFI:       return OCP_OWNER_WIFI;
        case PHY_OWNER_BLE:        return OCP_OWNER_BLE;
        case PHY_OWNER_IEEE802154: return OCP_OWNER_IEEE802154;
        case PHY_OWNER_NONE:       break;
    }
    return OCP_OWNER_NONE;
}

bool arbiter_stop_all(void)
{
    /* Teardown runs without the lock: it waits for the owner to release. */
    xSemaphoreTake(s_lock, portMAX_DELAY);
    arbiter_teardown_fn teardown = s_teardown;
    bool running = s_owner != PHY_OWNER_NONE;
    xSemaphoreGive(s_lock);

    if (teardown) teardown();
    return running;
}
