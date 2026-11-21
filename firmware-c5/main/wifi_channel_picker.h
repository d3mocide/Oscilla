/*
 * wifi_channel_picker.h — D-UCB discounted multi-armed bandit channel picker
 * (DESIGN §6.3, docs/DECISIONS.md D-6).
 *
 * Pure C, zero ESP-IDF dependency, zero heap allocation; host-tested under
 * ASan/UBSan in firmware-c5/test/host/wifi_channel_picker_test.c.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_CHANNEL_PICKER_H
#define OSCILLA_WIFI_CHANNEL_PICKER_H

#include <stddef.h>
#include <stdint.h>

#include "ocp_wifi_regions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t   count;
    float    n[OCP_WIFI_REGION_MAX_CHANNELS];
    float    x[OCP_WIFI_REGION_MAX_CHANNELS];
    uint32_t visits[OCP_WIFI_REGION_MAX_CHANNELS];
} wifi_channel_picker_t;

/* Reset for a new session over `count` active channels (clamped to
 * [1, OCP_WIFI_REGION_MAX_CHANNELS]). Always starts at channel index 0. */
void wifi_channel_picker_reset(wifi_channel_picker_t *p, size_t count);

/* Record `hits` observed during the dwell on `finished_idx` and return the
 * next channel index in [0, count). Visits every channel once first, then
 * selects by discounted UCB1 so busy channels earn more dwell while quiet
 * channels remain periodically revisited and moving traffic adapts. */
size_t wifi_channel_picker_step(wifi_channel_picker_t *p, size_t finished_idx, uint32_t hits);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_WIFI_CHANNEL_PICKER_H */
