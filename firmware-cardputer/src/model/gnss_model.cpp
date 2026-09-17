/*
 * gnss_model.cpp — see gnss_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/gnss_model.h"
#include "model/number_parse.h"

#include <climits>
#include <cmath>

namespace model {

namespace {

bool parseDigits(const std::string &s, int *out)
{
    if (s.empty()) return false;
    int value = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        if (value > (INT_MAX - (c - '0')) / 10) return false;
        value = value * 10 + (c - '0');
    }
    *out = value;
    return true;
}

/* NMEA lat/lon: `deg_digits` fixed integer degree digits, then minutes
 * (its own two integer digits plus a fractional part) — e.g. lat
 * "4717.11399" (2-digit deg) or lon "00833.91590" (3-digit deg). */
bool parseLatLon(const std::string &raw, const std::string &hemi, int deg_digits, double *out)
{
    if (raw.size() <= static_cast<size_t>(deg_digits) || hemi.size() != 1) return false;
    int deg = 0;
    double min = 0.0;
    if (!parseDigits(raw.substr(0, deg_digits), &deg)) return false;
    if (!parseFiniteDouble(raw.substr(deg_digits), &min)) return false;
    if (min < 0.0 || min >= 60.0) return false;

    double v = static_cast<double>(deg) + min / 60.0;
    char h = hemi[0];
    if (h == 'S' || h == 'W') v = -v;
    else if (h != 'N' && h != 'E') return false;
    double limit = (h == 'N' || h == 'S') ? 90.0 : 180.0;
    if (!std::isfinite(v) || v < -limit || v > limit) return false;
    *out = v;
    return true;
}

bool parseUtc(const std::string &raw, int *h, int *m, int *s)
{
    if (!h || !m || !s || raw.size() < 6) return false;
    for (size_t i = 0; i < 6; ++i) {
        if (raw[i] < '0' || raw[i] > '9') return false;
    }
    if (raw.size() > 6) {
        if (raw[6] != '.' || raw.size() == 7) return false;
        for (size_t i = 7; i < raw.size(); ++i) {
            if (raw[i] < '0' || raw[i] > '9') return false;
        }
    }
    *h = (raw[0] - '0') * 10 + (raw[1] - '0');
    *m = (raw[2] - '0') * 10 + (raw[3] - '0');
    *s = (raw[4] - '0') * 10 + (raw[5] - '0');
    return *h < 24 && *m < 60 && *s < 60;
}

/* NMEA RMC date field: fixed "ddmmyy", no separators. yy -> 2000+yy (see
 * gnss_model.h for why). Rejects anything not exactly 6 digits or with an
 * out-of-range day/month — never trust a partial field. */
bool parseNmeaDate(const std::string &raw, int *year, int *month, int *day)
{
    if (raw.size() != 6) return false;
    for (char c : raw) {
        if (c < '0' || c > '9') return false;
    }
    int dd = (raw[0] - '0') * 10 + (raw[1] - '0');
    int mm = (raw[2] - '0') * 10 + (raw[3] - '0');
    int yy = (raw[4] - '0') * 10 + (raw[5] - '0');
    if (mm < 1 || mm > 12) return false;
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int full_year = 2000 + yy;
    int max_day = days[mm - 1];
    if (mm == 2 && full_year % 4 == 0) max_day = 29;
    if (dd < 1 || dd > max_day) return false;
    *day = dd;
    *month = mm;
    *year = 2000 + yy;
    return true;
}

}  // namespace

void GnssModel::absorb(const gnss::Sentence &s, uint32_t now_ms)
{
    has_seen_any_ = true;
    last_sentence_at_ms_ = now_ms;

    if (s.type == "GGA") absorbGga(s, now_ms);
    else if (s.type == "RMC") absorbRmc(s, now_ms);
    /* Any other recognized-or-not type: already counted as UART activity above. */
}

/* $--GGA,time,lat,N/S,lon,E/W,quality,numSV,HDOP,alt,M,geoidSep,M,age,stn */
void GnssModel::absorbGga(const gnss::Sentence &s, uint32_t now_ms)
{
    if (s.fields.size() < 9) return;

    int quality = -1;
    (void)parseDigits(s.fields[5], &quality);
    fix_.valid = quality > 0;
    int utc_h = 0, utc_m = 0, utc_s = 0;
    if (parseUtc(s.fields[0], &utc_h, &utc_m, &utc_s)) fix_.utc = s.fields[0];

    if (!fix_.valid) return;   /* no-fix report: don't overwrite stale-but-last-known fields */

    double lat = 0.0, lon = 0.0, alt = 0.0, hdop = 0.0;
    bool have_lat = parseLatLon(s.fields[1], s.fields[2], 2, &lat);
    bool have_lon = parseLatLon(s.fields[3], s.fields[4], 3, &lon);
    bool have_alt = parseFiniteDouble(s.fields[8], &alt);
    bool have_hdop = s.fields[7].empty() ? false : parseFiniteDouble(s.fields[7], &hdop);
    have_hdop = have_hdop && hdop >= 0.0;
    if (!have_lat || !have_lon) {
        fix_.valid = false;
        return;   /* claims a fix but the position is unparsable: don't trust it */
    }

    fix_.lat_deg = lat;
    fix_.lon_deg = lon;
    if (have_alt) fix_.alt_m = alt;
    if (have_hdop) fix_.hdop = static_cast<float>(hdop);

    ever_fixed_ = true;
    last_fix_at_ms_ = now_ms;
}

/* $--RMC,time,status,lat,N/S,lon,E/W,speed,course,date,magvar,E/W[,mode] */
void GnssModel::absorbRmc(const gnss::Sentence &s, uint32_t now_ms)
{
    if (s.fields.size() < 6) return;

    bool status_valid = s.fields[1] == "A";
    fix_.valid = status_valid;
    int utc_h = 0, utc_m = 0, utc_s = 0;
    if (parseUtc(s.fields[0], &utc_h, &utc_m, &utc_s)) fix_.utc = s.fields[0];

    /* Date is parsed independent of status: a receiver's clock is commonly
     * RTC-backed and keeps a real calendar date even with no current fix
     * (gnss_model.h). Malformed/absent date leaves the last known one. */
    if (s.fields.size() >= 9) {
        int year = 0, month = 0, day = 0;
        if (parseNmeaDate(s.fields[8], &year, &month, &day)) {
            fix_.year = year;
            fix_.month = month;
            fix_.day = day;
        }
    }

    if (!status_valid) return;

    double lat = 0.0, lon = 0.0;
    bool have_lat = parseLatLon(s.fields[2], s.fields[3], 2, &lat);
    bool have_lon = parseLatLon(s.fields[4], s.fields[5], 3, &lon);
    if (!have_lat || !have_lon) {
        fix_.valid = false;
        return;
    }

    /* RMC carries neither altitude nor HDOP: leave whatever GGA last set. */
    fix_.lat_deg = lat;
    fix_.lon_deg = lon;

    ever_fixed_ = true;
    last_fix_at_ms_ = now_ms;
}

bool GnssModel::hasUartData(uint32_t now_ms) const
{
    if (!has_seen_any_) return false;
    return (now_ms - last_sentence_at_ms_) < kNoDataTimeoutMs;
}

uint32_t GnssModel::fixAgeMs(uint32_t now_ms) const
{
    if (!ever_fixed_) return 0;
    return now_ms - last_fix_at_ms_;
}

GnssState GnssModel::state(uint32_t now_ms) const
{
    /* Order matters: a silent receiver is "no data" whatever the last fix
     * said, or a stale valid flag would outlive the link that produced it. */
    if (!hasUartData(now_ms)) return GnssState::NoUartData;
    if (fix_.valid)           return GnssState::Fixed;
    return ever_fixed_ ? GnssState::FixLost : GnssState::Searching;
}

bool splitUtcTime(const std::string &utc, int *h, int *m, int *s)
{
    *h = *m = *s = 0;
    return parseUtc(utc, h, m, s);
}

const char *gnssStateName(GnssState s)
{
    switch (s) {
        case GnssState::NoUartData: return "no data";
        case GnssState::Searching:  return "searching";
        case GnssState::FixLost:    return "fix lost";
        case GnssState::Fixed:      return "fix";
    }
    return "?";
}

}  // namespace model
