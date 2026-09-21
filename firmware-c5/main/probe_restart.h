/*
 * probe_restart.h — deferred esp_restart() request, held off while LoRa RX
 * is running.
 *
 * The C5's IEEE 802.15.4 driver shares Wi-Fi/BLE's PHY and coexistence
 * state; esp_ieee802154_disable() can leave Wi-Fi silently returning zero
 * results afterward, with no public ESP-IDF API to clear it short of a
 * real boot (github.com/espressif/esp-matter/issues/1851 documents the
 * same missing-unregister defect on this chip, via BT/coex). zig_recon.c
 * requests a restart here instead of calling esp_restart() itself, because
 * LoRa (a separate lane, DESIGN §6.2) must survive a PHY-tool stop.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_PROBE_RESTART_H
#define OSCILLA_PROBE_RESTART_H

/* Mark a restart as owed, then attempt it immediately (see
 * probe_restart_if_safe). */
void probe_restart_request(void);

/* Honor a pending restart now, unless LoRa RX is running. Call again
 * whenever something might have made it safe — e.g. LoRa stopping. */
void probe_restart_if_safe(void);

#endif /* OSCILLA_PROBE_RESTART_H */
