/*
 * wifi_sniff.h — passive promiscuous sniffer: AP<->client map + probe-request
 * capture (OCP-SPEC §10.4).
 *
 * Receive-only: one promiscuous RX callback and a channel hopper, nothing
 * here ever transmits (D-8). tools/check_rx_only.py enforces it project-wide.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_SNIFF_H
#define OSCILLA_WIFI_SNIFF_H

#include "esp_err.h"

#define SNIFF_CLIENTS_MAX     128   /* one [CLIENTS] frame, no paging (§10.4) */
#define SNIFF_PROBES_MAX      192   /* one [PROBES] frame, no paging (§10.4) */

esp_err_t wifi_sniff_init(void);

/* start_sniffer: acquires the PHY, replies [SNIFF] immediately, then streams
 * [EVT] kind=sniff|client|probe (OCP-SPEC §10.4) until `stop`. */
void wifi_cmd_start_sniffer(void);

/* show_clients / show_probes: snapshot dump of the tables built up since the
 * last start_sniffer. Available whether or not the sniffer is still running. */
void wifi_cmd_show_clients(void);
void wifi_cmd_show_probes(void);

#endif /* OSCILLA_WIFI_SNIFF_H */
