/*
 * gnss_model.h — the deck's current-fix service (P4, DESIGN §9.1): absorbs
 * parsed NMEA sentences (gnss::NmeaParser) into the GPS fix record and keeps
 * it fresh. Framework-agnostic; host-tested in test/host/gnss_model_test.cpp.
 *
 * Rev D §6 in two sentences: "No fix is different from no UART data", and
 * don't assume UBX. This model tracks both states independently — a valid
 * checksum on *any* sentence type counts as UART activity even if the model
 * doesn't otherwise interpret it, while `valid`/`ageMs()` come only from
 * GGA/RMC content — and never assumes anything beyond standard NMEA output.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "gnss/nmea_parser.h"

namespace model {

struct GnssFix {
    double lat_deg = 0.0;
    double lon_deg = 0.0;
    double alt_m = 0.0;
    float hdop = 0.0f;
    std::string utc;      /* raw "hhmmss.ss" from the last sentence that set it */
    bool valid = false;   /* the *most recent* GGA/RMC's own fix/status flag */
};

class GnssModel {
public:
    /* No sentence of any kind in this long => treat the link as dead, not
     * merely fix-less. Comfortably above a 1 Hz fix's normal cadence. */
    static constexpr uint32_t kNoDataTimeoutMs = 3000;

    /* Feed one checksum-valid sentence from NmeaParser. Unrecognized types
     * are accepted (they still count as UART activity) and otherwise
     * ignored. Malformed GGA/RMC fields leave the stored fix untouched —
     * never trust a partial sentence, same posture as LoraModel::absorbEvent. */
    void absorb(const gnss::Sentence &s, uint32_t now_ms);

    /* Nothing has arrived on the GNSS UART recently: distinct from "no fix". */
    bool hasUartData(uint32_t now_ms) const;

    /* The last GGA/RMC's own report: is *this instant's* fix current. */
    bool hasFix() const { return fix_.valid; }

    /* Has a valid fix ever been obtained this session (independent of
     * whether the current instant still has one). */
    bool everFixed() const { return ever_fixed_; }

    /* Milliseconds since lat/lon/alt were last updated from a valid report.
     * Only meaningful once everFixed() — 0 until then. */
    uint32_t fixAgeMs(uint32_t now_ms) const;

    const GnssFix &fix() const { return fix_; }

private:
    void absorbGga(const gnss::Sentence &s, uint32_t now_ms);
    void absorbRmc(const gnss::Sentence &s, uint32_t now_ms);

    GnssFix fix_;
    bool ever_fixed_ = false;
    bool has_seen_any_ = false;
    uint32_t last_sentence_at_ms_ = 0;
    uint32_t last_fix_at_ms_ = 0;
};

}  // namespace model
