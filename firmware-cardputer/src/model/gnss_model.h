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
 * Calendar date comes only from RMC's date field (GGA carries time but no
 * date) and is parsed independent of RMC's own A/V status — a receiver's
 * clock is commonly RTC-backed and keeps reporting a real date even with no
 * current fix, a different signal from position validity. NMEA's date field
 * is a 2-digit year; per the same convention nearly every consumer NMEA
 * parser uses (e.g. TinyGPS++), it's read as 2000+yy — correct through 2099,
 * not receiver-specific, not a guess particular to this parser.
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
    int year = 0;         /* 0 = never set. RMC's date field only (GGA has none) */
    int month = 0;        /* 1-12 */
    int day = 0;          /* 1-31 */
    bool valid = false;   /* the *most recent* GGA/RMC's own fix/status flag */
};

/* The four states the deck must tell apart. Rev D §6 requires "no fix" and
 * "no UART data" to be distinct; P4's exit gate additionally exercises
 * losing a fix (antenna unplugged) with the receiver still talking, which is
 * a different signal from never having had one. */
enum class GnssState : uint8_t {
    NoUartData,   /* nothing on the UART recently: unwired, wrong baud, dead */
    Searching,    /* receiver talking, no fix yet this session */
    FixLost,      /* had a fix, current report is invalid */
    Fixed,        /* current report is valid */
};

const char *gnssStateName(GnssState s);

/* Splits GnssFix::utc ("hhmmss.ss", the raw NMEA field) into whole hours,
 * minutes and seconds. False — and 0/0/0, never a partial guess — if the
 * field is too short or not all digits. Lives here rather than in the
 * wardrive logger so it is host-testable (sd_storage.h's own rule). */
bool splitUtcTime(const std::string &utc, int *h, int *m, int *s);

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

    /* Collapses hasUartData()/hasFix()/everFixed() into the one state a view
     * or logger should branch on, so they can't disagree about it. */
    GnssState state(uint32_t now_ms) const;

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
