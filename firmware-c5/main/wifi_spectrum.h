/*
 * wifi_spectrum.h — passive per-channel activity metering (OCP-SPEC §10.6):
 * `channel_view` (hop all channels, one live reading each) and
 * `packet_monitor <ch>` (lock to one channel, packets/s). Both feed the
 * deck's "Spectrum" view (DESIGN §7.2).
 *
 * Neither command has a snapshot-dump verb: the `[EVT] kind=chan` stream is
 * the whole story, self-healing on a dropped event since every channel gets
 * a fresh reading again next cycle (channel_view) or next second
 * (packet_monitor) — unlike the sniffer's new-pairing-only events, a stale
 * bar just waits for its next refresh rather than being lost for good.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_SPECTRUM_H
#define OSCILLA_WIFI_SPECTRUM_H

#include "esp_err.h"

#define PACKET_MONITOR_INTERVAL_MS 1000   /* 1 s window: pkts this window == pkts/s */

esp_err_t wifi_spectrum_init(void);

/* channel_view: replies [CHAN] with the starting channel, then streams
 * [EVT] kind=chan ch=<ch> pkts=<n> once per hop (WIFI_CHAN_DWELL_MS) for
 * whichever channel dwell just finished, forever, until `stop`. */
void wifi_cmd_channel_view(void);

/* packet_monitor <ch>: replies [CFG] with the (validated) channel, then
 * streams [EVT] kind=chan ch=<ch> pkts=<n> once per second — a live
 * packets/s reading for that one fixed channel — until `stop`. */
void wifi_cmd_packet_monitor(int argc, char **argv);

#endif /* OSCILLA_WIFI_SPECTRUM_H */
