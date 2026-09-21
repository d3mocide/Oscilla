/*
 * probe_restart.c — see probe_restart.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "probe_restart.h"

#include <stdbool.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lora_radio.h"

static volatile bool s_pending;

void probe_restart_request(void)
{
    s_pending = true;
    probe_restart_if_safe();
}

void probe_restart_if_safe(void)
{
    if (!s_pending || lora_radio_is_running()) return;
    s_pending = false;
    vTaskDelay(pdMS_TO_TICKS(50));   /* let the ack drain (matches OCP_VID_REBOOT) */
    esp_restart();
}
