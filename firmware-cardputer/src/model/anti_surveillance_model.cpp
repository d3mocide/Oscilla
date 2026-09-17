/*
 * anti_surveillance_model.cpp — see anti_surveillance_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/anti_surveillance_model.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

#include "ocp.h"

namespace model {

namespace {

constexpr double kEarthRadiusM = 6371000.0;
constexpr double kPi = 3.14159265358979323846;

bool hexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

bool looksLikeMac(const std::string &s)
{
    if (s.size() != 17) return false;
    for (size_t i = 0; i < s.size(); i++) {
        if (i % 3 == 2) {
            if (s[i] != ':') return false;
        } else if (!hexDigit(s[i])) {
            return false;
        }
    }
    return true;
}

bool toRssi(const std::string &s, int *out)
{
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long value = std::strtol(s.c_str(), &end, 10);
    if (errno || *end || value < -128 || value > 127) return false;
    *out = static_cast<int>(value);
    return true;
}

double radians(double degrees)
{
    return degrees * kPi / 180.0;
}

double distanceM(double lat1, double lon1, double lat2, double lon2)
{
    const double dlat = radians(lat2 - lat1);
    const double dlon = radians(lon2 - lon1);
    const double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
                     std::cos(radians(lat1)) * std::cos(radians(lat2)) *
                     std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
    const double bounded = a > 1.0 ? 1.0 : a;
    return kEarthRadiusM * 2.0 * std::atan2(std::sqrt(bounded), std::sqrt(1.0 - bounded));
}

}  // namespace

void AntiSurveillanceModel::begin()
{
    clear();
    active_ = true;
}

void AntiSurveillanceModel::clear()
{
    trackers_.clear();
    trackers_.shrink_to_fit();
    active_ = false;
    position_valid_ = false;
    lat_deg_ = lon_deg_ = 0.0;
    position_at_ms_ = 0;
    alerts_ = 0;
}

void AntiSurveillanceModel::observePosition(double lat_deg, double lon_deg, bool usable,
                                            uint32_t fix_age_ms, uint32_t now_ms)
{
    if (!active_) return;
    if (!usable || fix_age_ms > kMaxFixAgeMs || !std::isfinite(lat_deg) ||
        !std::isfinite(lon_deg) || lat_deg < -90.0 || lat_deg > 90.0 ||
        lon_deg < -180.0 || lon_deg > 180.0) {
        position_valid_ = false;
        return;
    }
    lat_deg_ = lat_deg;
    lon_deg_ = lon_deg;
    position_at_ms_ = now_ms - fix_age_ms;
    position_valid_ = true;
}

AntiTracker *AntiSurveillanceModel::findOrInsert(const std::string &mac)
{
    for (auto &tracker : trackers_) {
        if (tracker.mac == mac) return &tracker;
    }
    if (trackers_.size() >= kMaxTrackers) return nullptr;
    trackers_.push_back(AntiTracker{});
    trackers_.back().mac = mac;
    return &trackers_.back();
}

void AntiSurveillanceModel::absorbEvent(const ocp::Item &evt, uint32_t now_ms)
{
    if (!active_) return;
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_AIRTAG) return;

    const auto *mac = evt.get(OCP_K_MAC);
    const auto *rssi_s = evt.get(OCP_K_RSSI);
    int rssi = 0;
    if (!mac || !looksLikeMac(*mac) || !rssi_s || !toRssi(*rssi_s, &rssi)) return;

    AntiTracker *tracker = findOrInsert(*mac);
    if (!tracker) return;
    tracker->rssi = rssi;
    tracker->sightings++;
    tracker->last_seen_ms = now_ms;

    if (!position_valid_) return;
    /* A stale last fix is not a movement observation. Keep the tracker row,
     * but require a fresh fix before the next leg can count. */
    if (now_ms - position_at_ms_ > kMaxFixAgeMs) return;

    if (!tracker->has_anchor) {
        /* The first sighting anchors this tracker to the current fix. */
        tracker->anchor_lat_deg = lat_deg_;
        tracker->anchor_lon_deg = lon_deg_;
        tracker->has_anchor = true;
        return;
    }

    const double step = distanceM(tracker->anchor_lat_deg, tracker->anchor_lon_deg,
                                  lat_deg_, lon_deg_);
    if (step < kMovementStepM) return;

    tracker->anchor_lat_deg = lat_deg_;
    tracker->anchor_lon_deg = lon_deg_;
    tracker->movement_legs++;
    tracker->traveled_m += step;
    if (!tracker->alert && tracker->movement_legs >= kAlertMovementLegs) {
        tracker->alert = true;
        alerts_++;
    }
}

const AntiTracker *AntiSurveillanceModel::latestAlert() const
{
    const AntiTracker *latest = nullptr;
    for (const auto &tracker : trackers_) {
        if (!tracker.alert || (latest && tracker.last_seen_ms <= latest->last_seen_ms)) continue;
        latest = &tracker;
    }
    return latest;
}

}  // namespace model
