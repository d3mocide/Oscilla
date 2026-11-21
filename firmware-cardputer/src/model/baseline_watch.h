/*
 * baseline_watch.h — passive Wi-Fi + 802.15.4 baseline & anomaly watch (WP-11).
 *
 * Strictly receive-only, deck-local comparison against an operator-captured
 * snapshot of known APs and 802.15.4 PANs. Flags conservative, truthful
 * deltas (never claims certainty):
 *   - NewAp: BSSID absent from baseline
 *   - EvilTwin: known baseline SSID broadcast from an unfamiliar BSSID
 *   - AuthDowngrade: known BSSID observed with weaker security tier
 *   - MfpDropped: baseline MFP-required BSSID observed without MFP required
 *   - NewPan: unfamiliar 802.15.4 PAN ID
 *
 * Framework-agnostic pure C++; host-tested in test/host/baseline_watch_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "model/scan_model.h"
#include "model/zig_model.h"

namespace model {

struct BaselineAp {
    std::string bssid;
    std::string ssid;
    std::string auth;
    uint8_t ch = 0;
    bool mfp_required = false;
};

struct BaselinePan {
    std::string pan_id;
    std::string channels;
};

enum class AnomalyKind : uint8_t {
    NewAp = 0,
    NewPan = 1,
    MfpDropped = 2,
    AuthDowngrade = 3,
    EvilTwin = 4,
};

struct BaselineAnomaly {
    AnomalyKind kind = AnomalyKind::NewAp;
    std::string id;       /* BSSID or PAN ID */
    std::string detail;   /* SSID or security delta */
    uint32_t seen_ms = 0;
};

class BaselineWatch {
public:
    static constexpr size_t kMaxBaselineAps = 256;
    static constexpr size_t kMaxBaselinePans = 32;
    static constexpr size_t kMaxAnomalies = 64;

    static int authRank(const std::string &auth);
    static const char *kindShort(AnomalyKind kind);
    static const char *kindLabel(AnomalyKind kind);
    static bool isHighSeverity(AnomalyKind kind);

    void captureFrom(const ScanModel &scan, const ZigModel &zig);
    std::string serializeCsv() const;
    bool parseCsv(const std::string &csv);
    void clear();

    bool active() const { return !aps_.empty() || !pans_.empty(); }
    size_t apCount() const { return aps_.size(); }
    size_t panCount() const { return pans_.size(); }

    void evaluate(const ScanModel &scan, const ZigModel &zig, uint32_t now_ms);
    void evaluateInspect(const Inspect &in, uint32_t now_ms);

    const std::vector<BaselineAnomaly> &anomalies() const { return anomalies_; }
    const BaselineAnomaly *anomalyForBssid(const std::string &bssid) const;
    size_t highSeverityCount() const;
    size_t countOf(AnomalyKind kind) const;

private:
    void upsertAnomaly(AnomalyKind kind, const std::string &id, const std::string &detail, uint32_t now_ms);
    const BaselineAp *findAp(const std::string &bssid) const;
    bool hasSsidInBaseline(const std::string &ssid) const;
    bool hasPanInBaseline(const std::string &pan_id) const;

    std::vector<BaselineAp> aps_;
    std::vector<BaselinePan> pans_;
    std::vector<BaselineAnomaly> anomalies_;
};

}  // namespace model
