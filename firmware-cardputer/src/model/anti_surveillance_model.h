/*
 * anti_surveillance_model.h — deck-local movement correlation for passive
 * tracker sightings (OCP-SPEC §11.6). It never sends position over OCP or
 * writes tracker identifiers to storage; it keeps only a bounded RAM view.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct AntiTracker {
    std::string mac;
    int rssi = 0;
    uint32_t sightings = 0;
    uint8_t movement_legs = 0;
    double traveled_m = 0.0;
    uint32_t last_seen_ms = 0;
    bool alert = false;
    /* Last location at which this tracker was observed. These stay in RAM
     * only; the view uses the public movement counters above. */
    double anchor_lat_deg = 0.0;
    double anchor_lon_deg = 0.0;
    bool has_anchor = false;
};

class AntiSurveillanceModel {
public:
    static constexpr size_t kMaxTrackers = 32;
    static constexpr double kMovementStepM = 25.0;
    static constexpr uint8_t kAlertMovementLegs = 2;
    static constexpr uint32_t kMaxFixAgeMs = 10000;

    /* start_antisurveillance was issued: forget the old correlation. */
    void begin();

    /* [STOP] landed: keep the evidence for a last look, stop correlating. */
    void stop() { active_ = false; }

    /* Probe rebooted or a new session needs a clean slate. */
    void clear();

    /* Feed the freshest deck-local fix. Invalid or stale fixes are not used. */
    void observePosition(double lat_deg, double lon_deg, bool usable,
                         uint32_t fix_age_ms, uint32_t now_ms);

    /* Absorb [EVT] kind=airtag. The probe supplies no position. */
    void absorbEvent(const ocp::Item &evt, uint32_t now_ms);

    bool active() const { return active_; }
    uint16_t alertCount() const { return alerts_; }
    bool hasPosition() const { return position_valid_; }
    const std::vector<AntiTracker> &trackers() const { return trackers_; }

    /* Returns the newest tracker that has crossed the conservative movement
     * threshold twice, or nullptr when this session has no alert yet. */
    const AntiTracker *latestAlert() const;

private:
    AntiTracker *findOrInsert(const std::string &mac);

    std::vector<AntiTracker> trackers_;
    bool active_ = false;
    bool position_valid_ = false;
    double lat_deg_ = 0.0;
    double lon_deg_ = 0.0;
    uint32_t position_at_ms_ = 0;
    uint16_t alerts_ = 0;
};

}  // namespace model
