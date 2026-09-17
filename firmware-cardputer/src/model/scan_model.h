/*
 * scan_model.h — the deck's view of a Wi-Fi survey (OCP-SPEC §10).
 * Framework-agnostic; host-tested in test/host/model_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct ApRow {
    uint16_t idx = 0;
    std::string ssid;      /* raw bytes: re-escape before display */
    std::string bssid;
    uint8_t ch = 0;
    std::string auth;
    int rssi = 0;
    bool band5 = false;
};

struct Inspect {
    bool valid = false;
    bool aborted = false;
    uint16_t idx = 0;
    std::string bssid;
    uint8_t ch = 0;
    uint16_t beacons = 0;
    int rssi = 0;
    bool rsn = false, mfp_capable = false, mfp_required = false;
    uint64_t uptime_s = 0;
    uint16_t interval_ms = 0;
};

class ScanModel {
public:
    static constexpr size_t kMaxRows = 512;   /* deck RAM, no PSRAM */

    /* A scan was requested: forget the old one. */
    void begin();

    /* start_wifi_scan was (re)issued: forget the old list and fill it from
     * first-sighting network events instead of a paged snapshot. */
    void beginContinuous();

    /* [STOP] landed: keep the discovered rows for a last look. */
    void stop() { scanning_ = false; continuous_active_ = false; }

    /* Probe rebooted: its stored indices are gone, so ours are meaningless. */
    void clear();

    /* Absorb a [SCAN] frame. Returns the `first` to request next, or 0 when
     * the survey is complete (all pages, aborted, or kMaxRows reached). */
    uint16_t absorbPage(const ocp::Item &frame);

    /* Absorb an [EVT] kind=network, upserting by BSSID. */
    void absorbEvent(const ocp::Item &evt);

    void absorbInspect(const ocp::Item &frame);

    bool scanning() const { return scanning_; }
    bool continuousActive() const { return continuous_active_; }
    bool aborted() const { return aborted_; }
    uint16_t total() const { return total_; }        /* as reported by the probe */
    bool truncated() const { return total_ > rows_.size() && !scanning_ && !aborted_; }
    uint32_t elapsedMs() const { return elapsed_ms_; }
    uint16_t malformedRows() const { return malformed_; }
    const std::vector<ApRow> &rows() const { return rows_; }
    const Inspect &inspect() const { return inspect_; }

private:
    std::vector<ApRow> rows_;
    Inspect inspect_;
    bool scanning_ = false;
    bool continuous_active_ = false;
    bool aborted_ = false;
    uint16_t total_ = 0;
    uint16_t malformed_ = 0;
    uint32_t elapsed_ms_ = 0;
};

}  // namespace model
