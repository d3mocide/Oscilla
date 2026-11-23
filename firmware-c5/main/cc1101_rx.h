/*
 * cc1101_rx.h — CC1101 receive start/stop and RSSI (DESIGN §6.1). Runs on a
 * chip already configured by cc1101_config.c. Receive-only (D-8).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_RX_H
#define OSCILLA_CC1101_RX_H

#include <stdbool.h>
#include <stdint.h>

#include "cc1101_bus.h"

/* Typical RSSI_offset at 433 MHz for every listed data rate (SWRS061I Table 31). */
#define CC1101_RSSI_OFFSET_DB 74

/* Minimum spacing between RSSI reads. Back-to-back SPI raised the floor
 * 4-6 dB and produced false GDO0 edges on the bench (WORKLOG 2026-11-23). */
#define CC1101_RSSI_POLL_MIN_US 200u

/* Bound on IDLE->RX including calibration. */
#define CC1101_RX_START_TIMEOUT_US 5000u

/* Flush RX, calibrate (SCAL first when MCSM0.FS_AUTOCAL does not cover
 * IDLE->RX), SRX, then confirm MARCSTATE == RX. *calibrated (may be NULL)
 * reports whether a calibration was guaranteed for this start. */
cc1101_err_t cc1101_rx_start(const cc1101_bus_t *bus, bool *calibrated);

/* SIDLE and confirm IDLE. */
cc1101_err_t cc1101_rx_stop(const cc1101_bus_t *bus);

/* Current RSSI in dBm (SWRS061I §17.3); valid only in RX. */
cc1101_err_t cc1101_rx_rssi(const cc1101_bus_t *bus, int16_t *dbm);

/* FIFO sampling (cc1101_profile_make_fifo): read all but the last RX FIFO
 * byte, which must stay until the next byte lands (SWRS061I §20). On RX
 * FIFO overflow, restart RX (recalibrating), set *overflowed and return no
 * bytes. cap must be at least 1. */
cc1101_err_t cc1101_rx_drain(const cc1101_bus_t *bus, uint8_t *out, size_t cap, size_t *n,
                             bool *overflowed);

/* Raw RSSI register to dBm in whole dB, rounded toward -inf (SWRS061I §17.3). */
int16_t cc1101_rssi_dbm(uint8_t raw, int offset_db);

#endif /* OSCILLA_CC1101_RX_H */
