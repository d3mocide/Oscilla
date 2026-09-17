/*
 * bt_model.h — the deck's view of BLE passive scanning (OCP-SPEC §11):
 * scan_bt's device table and scan_airtag's Find My / AirTag sighting
 * stream. Framework-agnostic; host-tested in test/host/bt_model_test.cpp.
 *
 * Named "Bt", not "Ble": tools/check_rx_only.py bans any `ble_`/`esp_ble_`/
 * `NimBLE` identifier from deck source outright (deck radios stay off in
 * v1, DESIGN §3) — this model only ever parses text the probe already
 * sent, but the wrong identifier prefix would still trip the tripwire on a
 * name collision, so the deck side spells it differently on purpose.
 *
 * [BLE] has no paging (OCP-SPEC §11.2): one absorbScan() call is always the
 * complete, final result, unlike ScanModel's absorbPage().
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct BtDevice {
    std::string mac;
    std::string name;    /* raw bytes, may be empty: re-escape before display */
    std::string mfr;     /* 4 lowercase hex digits, or empty */
    bool tracker = false;
    int rssi = 0;
    uint32_t n = 0;
};

struct BtTrackerHit {
    std::string mac;
    int rssi = 0;
};

class BtModel {
public:
    static constexpr size_t kMaxDevices = 96;         /* matches the probe's BLE_DEVICES_MAX */
    static constexpr size_t kMaxTrackerHits = 64;      /* newest-first, oldest dropped past this */

    /* scan_bt was (re)issued: forget the old device list. */
    void beginScan();

    /* start_ble_scan was (re)issued: forget the old accumulated table —
     * same fresh-session posture as beginScan(), just filled by events
     * instead of one snapshot frame. */
    void beginContinuous();

    /* scan_airtag was (re)issued: forget the old sighting log. */
    void beginAirtag();

    /* start_antisurveillance has its own movement model, but the Beacons
     * card still shows the current tracker ticker; reset that ticker without
     * claiming the ordinary scan_airtag mode is active. */
    void resetTrackerLog();

    /* [STOP] landed: scanning/continuous/airtag all go inactive; stored
     * data persists for a last look, same posture as ContactsModel/
     * DeauthModel. */
    void stop();

    /* Probe rebooted: nothing here is trustworthy any more. */
    void clear();

    /* Absorb a [BLE] frame — the complete device table, not a page. */
    void absorbScan(const ocp::Item &frame);

    /* Absorb an [EVT] kind=airtag or kind=ble (the latter upserts one row
     * into the same devices() table absorbScan() populates — a script or
     * view watching that list doesn't need to know which command filled
     * it). Both are no-ops for a kind they don't recognize. */
    void absorbEvent(const ocp::Item &evt);

    bool scanning() const { return scanning_; }
    bool continuousActive() const { return continuous_active_; }
    bool airtagActive() const { return airtag_active_; }
    uint16_t malformedRows() const { return malformed_; }
    const std::vector<BtDevice> &devices() const { return devices_; }

    uint32_t trackerCount() const { return tracker_total_; }
    const std::vector<BtTrackerHit> &trackerHits() const { return tracker_hits_; }

private:
    BtDevice *findOrInsertDevice(const std::string &mac);

    std::vector<BtDevice> devices_;
    uint16_t malformed_ = 0;
    bool scanning_ = false;
    bool continuous_active_ = false;

    bool airtag_active_ = false;
    uint32_t tracker_total_ = 0;
    std::vector<BtTrackerHit> tracker_hits_;   /* index 0 = most recent */
};

}  // namespace model
