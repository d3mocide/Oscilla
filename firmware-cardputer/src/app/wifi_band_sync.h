/*
 * wifi_band_sync.h — the deck's Wi-Fi band scope setting and when to push it
 * to the probe (`set_band`, OCP-SPEC §10.9).
 *
 * Mirrors wifi_region_sync.h: saved on SD, pushed whenever the probe reports
 * a different `band=` in [HELLO]/[STATUS] and nothing holds the PHY lane.
 * A probe reporting no `band=` predates the verb: never push to it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ocp.h"

#include <cstdint>
#include <string>

namespace app {

constexpr uint32_t kBandRetryMs = 10000;

inline bool isValidWifiBand(const std::string &code)
{
    return code == OCP_BAND_AUTO || code == OCP_BAND_24 || code == OCP_BAND_5;
}

inline std::string bandOrDefault(const std::string &code)
{
    return isValidWifiBand(code) ? code : std::string(OCP_BAND_AUTO);
}

/* Cycle AUTO -> 24 -> 5 -> AUTO (step > 0) or reverse (step < 0). */
inline std::string cycleBand(const std::string &code, int step)
{
    static constexpr const char *kBands[] = { OCP_BAND_AUTO, OCP_BAND_24, OCP_BAND_5 };
    constexpr size_t n = sizeof(kBands) / sizeof(kBands[0]);
    size_t at = 0;
    for (size_t i = 0; i < n; i++) {
        if (code == kBands[i]) at = i;
    }
    return kBands[(at + n + (step < 0 ? n - 1 : 1)) % n];
}

inline const char *bandDisplayLabel(const std::string &code)
{
    if (code == OCP_BAND_24) return "2.4G";
    if (code == OCP_BAND_5)  return "5G";
    return "AUTO";
}

inline bool bandPushDue(const std::string &desired, const std::string &probe_band,
                        bool phy_idle, bool attempted, uint32_t since_attempt_ms)
{
    if (probe_band.empty() || probe_band == desired || !phy_idle) return false;
    return !attempted || since_attempt_ms >= kBandRetryMs;
}

enum class BandState { Offline, Unsupported, Applied, Pending };

inline BandState bandState(bool link_ready, const std::string &desired,
                           const std::string &probe_band)
{
    if (!link_ready) return BandState::Offline;
    if (probe_band.empty()) return BandState::Unsupported;
    return probe_band == desired ? BandState::Applied : BandState::Pending;
}

}  // namespace app
