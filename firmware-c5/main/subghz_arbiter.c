/*
 * subghz_arbiter.c — see subghz_arbiter.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "subghz_arbiter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lock;
static subghz_owner_t s_owner = SUBGHZ_OWNER_NONE;
static subghz_teardown_fn s_teardown;

esp_err_t subghz_arbiter_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t subghz_arbiter_acquire(subghz_owner_t who, subghz_teardown_fn teardown)
{
    esp_err_t err = ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_owner == SUBGHZ_OWNER_NONE) {
        s_owner = who;
        s_teardown = teardown;
        err = ESP_OK;
    }
    xSemaphoreGive(s_lock);
    return err;
}

void subghz_arbiter_release(subghz_owner_t who)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_owner == who) {
        s_owner = SUBGHZ_OWNER_NONE;
        s_teardown = NULL;
    }
    xSemaphoreGive(s_lock);
}

subghz_owner_t subghz_arbiter_owner(void)
{
    return s_owner;
}

bool subghz_arbiter_stop(void)
{
    /* Teardown runs without the lock: it waits for the owner to release. */
    xSemaphoreTake(s_lock, portMAX_DELAY);
    subghz_teardown_fn teardown = s_teardown;
    bool running = s_owner != SUBGHZ_OWNER_NONE;
    xSemaphoreGive(s_lock);

    if (teardown) teardown();
    return running;
}
