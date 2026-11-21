/*
 * wifi_channel_picker.c — see wifi_channel_picker.h.
 *
 * Discounted UCB1 (Garivier & Moulines): every step decays prior counts and
 * rewards by GAMMA so non-stationary traffic shifts quickly, and saturates
 * per-dwell hits into [0, 1) so no single busy channel starves quiet ones.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_channel_picker.h"

#include <math.h>
#include <string.h>

#define DUCB_GAMMA       0.95f
#define DUCB_EXPLORE_C   0.90f
#define DUCB_HALF_SAT    2.0f
#define DUCB_MIN_N       1e-6f

void wifi_channel_picker_reset(wifi_channel_picker_t *p, size_t count)
{
    memset(p, 0, sizeof *p);
    if (count == 0) count = 1;
    if (count > OCP_WIFI_REGION_MAX_CHANNELS) count = OCP_WIFI_REGION_MAX_CHANNELS;
    p->count = count;
}

size_t wifi_channel_picker_step(wifi_channel_picker_t *p, size_t finished_idx, uint32_t hits)
{
    if (!p || p->count <= 1) return 0;

    /* Discount all arms so old observations fade in moving environments. */
    for (size_t i = 0; i < p->count; i++) {
        p->n[i] *= DUCB_GAMMA;
        p->x[i] *= DUCB_GAMMA;
    }

    if (finished_idx < p->count) {
        float reward = hits ? ((float)hits / ((float)hits + DUCB_HALF_SAT)) : 0.0f;
        p->n[finished_idx] += 1.0f;
        p->x[finished_idx] += reward;
        p->visits[finished_idx]++;
    }

    /* Initial sweep: guarantee every channel is sampled once before UCB. */
    for (size_t i = 0; i < p->count; i++) {
        if (p->visits[i] == 0) return i;
    }

    float n_total = 0.0f;
    for (size_t i = 0; i < p->count; i++) n_total += p->n[i];
    if (n_total < 1.0f) n_total = 1.0f;
    float log_total = logf(n_total);
    if (log_total < 0.0f) log_total = 0.0f;

    /* Start scanning after finished_idx so equal scores round-robin cleanly. */
    size_t best_idx = (finished_idx + 1) % p->count;
    float best_score = -1.0f;
    for (size_t step = 0; step < p->count; step++) {
        size_t i = (finished_idx + 1 + step) % p->count;
        float ni = p->n[i] > DUCB_MIN_N ? p->n[i] : DUCB_MIN_N;
        float mean = p->x[i] / ni;
        float bonus = DUCB_EXPLORE_C * sqrtf(log_total / ni);
        float score = mean + bonus;
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    return best_idx;
}
