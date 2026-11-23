/*
 * cc1101_rx.c — see cc1101_rx.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_rx.h"

#define IDLE_TIMEOUT_US 5000u
#define FS_AUTOCAL_IDLE_TO_RX 1u   /* MCSM0[5:4] = 01 (SWRS061I §29.1) */

int16_t cc1101_rssi_dbm(uint8_t raw, int offset_db)
{
    const int dec = raw >= 128 ? (int)raw - 256 : (int)raw;
    const int half = dec >= 0 ? dec / 2 : -((1 - dec) / 2);
    return (int16_t)(half - offset_db);
}

cc1101_err_t cc1101_rx_start(const cc1101_bus_t *bus, bool *calibrated)
{
    if (calibrated) *calibrated = false;
    if (!bus) return CC1101_ERR_ARG;

    uint8_t mcsm0 = 0;
    cc1101_err_t err = cc1101_read_reg(bus, CC1101_MCSM0, &mcsm0);
    if (err == CC1101_OK) err = cc1101_strobe(bus, CC1101_SIDLE, NULL);
    if (err == CC1101_OK) err = cc1101_wait_state(bus, CC1101_MARC_IDLE, IDLE_TIMEOUT_US, NULL);
    if (err == CC1101_OK) err = cc1101_strobe(bus, CC1101_SFRX, NULL);
    if (err != CC1101_OK) return err;

    /* An uncalibrated synthesizer leaves the receiver deaf (WORKLOG 2026-11-23). */
    if (((mcsm0 >> 4) & 0x3) != FS_AUTOCAL_IDLE_TO_RX) {
        err = cc1101_strobe(bus, CC1101_SCAL, NULL);
        if (err == CC1101_OK) err = cc1101_wait_state(bus, CC1101_MARC_IDLE, IDLE_TIMEOUT_US, NULL);
        if (err != CC1101_OK) return err;
    }

    err = cc1101_strobe(bus, CC1101_SRX, NULL);
    if (err == CC1101_OK) err = cc1101_wait_state(bus, CC1101_MARC_RX, CC1101_RX_START_TIMEOUT_US, NULL);
    if (err == CC1101_OK && calibrated) *calibrated = true;
    return err;
}

cc1101_err_t cc1101_rx_stop(const cc1101_bus_t *bus)
{
    if (!bus) return CC1101_ERR_ARG;
    cc1101_err_t err = cc1101_strobe(bus, CC1101_SIDLE, NULL);
    if (err == CC1101_OK) err = cc1101_wait_state(bus, CC1101_MARC_IDLE, IDLE_TIMEOUT_US, NULL);
    return err;
}

cc1101_err_t cc1101_rx_rssi(const cc1101_bus_t *bus, int16_t *dbm)
{
    if (!bus || !dbm) return CC1101_ERR_ARG;
    uint8_t raw = 0;
    const cc1101_err_t err = cc1101_read_status(bus, CC1101_RSSI, &raw);
    if (err == CC1101_OK) *dbm = cc1101_rssi_dbm(raw, CC1101_RSSI_OFFSET_DB);
    return err;
}
