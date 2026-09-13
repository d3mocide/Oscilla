/*
 * wifi_channels.h — the shared passive channel-hop list, used by every
 * promiscuous engine (wifi_sniff.c, wifi_deauth.c, wifi_spectrum.c): plain
 * round-robin, both bands.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_CHANNELS_H
#define OSCILLA_WIFI_CHANNELS_H

#include <stddef.h>
#include <stdint.h>

#define WIFI_CHAN_DWELL_MS 300   /* per channel */

/*
 * 2.4 GHz 1-13, plus the non-DFS 5 GHz channels (UNII-1 36-48, UNII-3
 * 149-165). DFS channels (UNII-2/2e, 52-140) are left out — tracked as
 * D-14, not silently dropped: esp_wifi_set_channel silently fails on a
 * channel the configured regulatory domain doesn't permit tuning to, and on
 * failure the radio stays on its previous channel while a caller's hop
 * index still advances, mislabelling captured frames with the channel it
 * *meant* to be on. DFS also carries its own radar-avoidance rules a
 * passive listener may still touch (unresearched); worth a real look
 * before adding them, not a default. D-UCB dwell weighting is deferred
 * (D-6); this is v1's "simple round-robin".
 *
 * Returns the fixed hop list and its length via `count`. The array is
 * static storage: the returned pointer is valid for the process lifetime.
 */
const uint8_t *wifi_channels(size_t *count);

#endif /* OSCILLA_WIFI_CHANNELS_H */
