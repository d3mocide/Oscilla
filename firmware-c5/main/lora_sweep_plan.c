/*
 * lora_sweep_plan.c — see lora_sweep_plan.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_sweep_plan.h"

#include <stdio.h>

#include "ocp.h"

bool lora_sweep_plan_valid(uint32_t start_khz, uint32_t step_khz, uint32_t bins)
{
    if (bins < LORA_SWEEP_MIN_BINS || bins > OCP_LORA_SWEEP_MAX_BINS) return false;
    if (step_khz < LORA_SWEEP_MIN_STEP_KHZ || step_khz > LORA_SWEEP_MAX_STEP_KHZ) return false;
    if (start_khz < LORA_SWEEP_MIN_FREQ_KHZ || start_khz > LORA_SWEEP_MAX_FREQ_KHZ) return false;

    uint64_t end_khz = (uint64_t)start_khz + (uint64_t)(bins - 1u) * (uint64_t)step_khz;
    return end_khz <= (uint64_t)LORA_SWEEP_MAX_FREQ_KHZ;
}

int8_t lora_rssi_inst_to_dbm(uint8_t raw)
{
    int dbm = -((int)raw / 2);
    if (dbm < -127) dbm = -127;
    if (dbm > 0) dbm = 0;
    return (int8_t)dbm;
}

bool lora_sweep_format_rssi(const int8_t *rssi, size_t n, char *out, size_t out_size)
{
    if (!rssi || !out || out_size == 0 || n == 0 || n > OCP_LORA_SWEEP_MAX_BINS) return false;
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        int w = snprintf(out + used, out_size - used, "%s%d", i ? "," : "", (int)rssi[i]);
        if (w < 0 || (size_t)w >= out_size - used) {
            out[0] = '\0';
            return false;
        }
        used += (size_t)w;
    }
    return true;
}
