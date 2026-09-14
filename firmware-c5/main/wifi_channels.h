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
 * D-14, not silently dropped. Verified against ESP-IDF's own regulatory
 * database and the actual FCC allocation table (D-14, 2026-09-14): this
 * exact split is correct for the US domain, and 47 CFR 15.407(h)(2) scopes
 * the DFS radar-detection obligation to a transmitter's "emission
 * bandwidth" — a receive-only device (D-8) is categorically outside it, so
 * that's no longer a blocker on its own.
 *
 * What's still open is narrower than "unresearched": every hop callback
 * already checks esp_wifi_set_channel's return and logs on failure, so a
 * hard error isn't silent. What's unconfirmed is whether a
 * regulatory-disallowed channel *always* surfaces as that checkable error,
 * or can return ESP_OK while silently not moving (the enforcement lives in
 * the closed-source Wi-Fi lib, invisible from any header) — if the latter,
 * the hop index would still advance and mislabel captured frames with the
 * channel it *meant* to be on. One bench test settles it: request channel
 * 52, log the return code, then confirm via esp_wifi_get_channel whether it
 * actually moved. See docs/DECISIONS.md D-14 for the full writeup.
 *
 * D-UCB dwell weighting is deferred (D-6); this is v1's "simple
 * round-robin".
 *
 * Returns the fixed hop list and its length via `count`. The array is
 * static storage: the returned pointer is valid for the process lifetime.
 */
const uint8_t *wifi_channels(size_t *count);

#endif /* OSCILLA_WIFI_CHANNELS_H */
