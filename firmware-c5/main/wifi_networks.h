/*
 * wifi_networks.h — continuous passive Wi-Fi AP/SSID discovery. It hops the
 * shared channel list and streams one event for each newly seen BSSID.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_NETWORKS_H
#define OSCILLA_WIFI_NETWORKS_H

#include "esp_err.h"

/* Creates the hop timer and bounded-table lock. */
esp_err_t wifi_networks_init(void);

/* Runs until `stop`; emits [EVT] kind=network on first sighting of each
 * BSSID. The event stream is intentionally the live record, like BLE. */
void wifi_cmd_start_network_scan(void);

#endif /* OSCILLA_WIFI_NETWORKS_H */
