/*
 * wifi_region_sync.h — the deck's Wi-Fi region setting and when to push it
 * to the probe (`set_region`, OCP-SPEC §10.8).
 *
 * The deck owns the setting (saved on SD); the probe forgets it on every
 * reboot and starts at OCP_WIFI_REGION_DEFAULT. The probe reports its
 * region as `region=` in [HELLO]/[STATUS], so the deck resends whenever the
 * two disagree and nothing holds the PHY lane (the probe refuses a region
 * change mid-tool). A probe that reports no region predates the verb: never
 * push to it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ocp_wifi_regions.h"

#include <cstdint>
#include <string>

namespace app {

constexpr uint32_t kRegionRetryMs = 10000;

/* A saved or typed code, or the default when it is not in the table. */
inline const ocp_wifi_region_t &regionOrDefault(const std::string &code)
{
    const ocp_wifi_region_t *r = ocp_wifi_region_find(code.c_str());
    return r ? *r : *ocp_wifi_region_find(OCP_WIFI_REGION_DEFAULT);
}

/* The neighbouring table entry, wrapping; `step` is +1 or -1. */
inline const ocp_wifi_region_t &cycleRegion(const std::string &code, int step)
{
    size_t n = 0;
    const ocp_wifi_region_t *t = ocp_wifi_region_table(&n);
    size_t at = 0;
    for (size_t i = 0; i < n; i++)
        if (code == t[i].code) at = i;
    return t[(at + n + (step < 0 ? n - 1 : 1)) % n];
}

inline bool regionPushDue(const std::string &desired, const std::string &probe_region,
                          bool phy_idle, bool attempted, uint32_t since_attempt_ms)
{
    if (probe_region.empty() || probe_region == desired || !phy_idle) return false;
    return !attempted || since_attempt_ms >= kRegionRetryMs;
}

enum class RegionState { Offline, Unsupported, Applied, Pending };

inline RegionState regionState(bool link_ready, const std::string &desired,
                               const std::string &probe_region)
{
    if (!link_ready) return RegionState::Offline;
    if (probe_region.empty()) return RegionState::Unsupported;
    return probe_region == desired ? RegionState::Applied : RegionState::Pending;
}

inline const char *regionStateLabel(RegionState state)
{
    switch (state) {
    case RegionState::Offline:     return "PROBE OFFLINE";
    case RegionState::Unsupported: return "PROBE TOO OLD";
    case RegionState::Applied:     return "APPLIED";
    case RegionState::Pending:     return "PENDING";
    }
    return "";
}

}  // namespace app
