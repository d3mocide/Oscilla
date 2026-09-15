/*
 * gnss_model.cpp — see gnss_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/gnss_model.h"

#include <cerrno>
#include <cstdlib>

namespace model {

namespace {

/* Whole-string strtod: no trailing garbage, no empty input. Same
 * never-trust-a-partial-field posture as lora_model.cpp's kvFloat. */
bool parseDouble(const std::string &s, double *out)
{
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    double v = std::strtod(s.c_str(), &end);
    if (errno || end != s.c_str() + s.size()) return false;
    *out = v;
    return true;
}

/* NMEA lat/lon: `deg_digits` fixed integer degree digits, then minutes
 * (its own two integer digits plus a fractional part) — e.g. lat
 * "4717.11399" (2-digit deg) or lon "00833.91590" (3-digit deg). */
bool parseLatLon(const std::string &raw, const std::string &hemi, int deg_digits, double *out)
{
    if (raw.size() <= static_cast<size_t>(deg_digits) || hemi.size() != 1) return false;
    double deg = 0.0, min = 0.0;
    if (!parseDouble(raw.substr(0, deg_digits), &deg)) return false;
    if (!parseDouble(raw.substr(deg_digits), &min)) return false;
    if (min < 0.0 || min >= 60.0) return false;

    double v = deg + min / 60.0;
    char h = hemi[0];
    if (h == 'S' || h == 'W') v = -v;
    else if (h != 'N' && h != 'E') return false;
    *out = v;
    return true;
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
    if (dd < 1 || dd > 31 || mm < 1 || mm > 12) return false;
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

    long quality = -1;
    if (!s.fields[5].empty()) {
        char *end = nullptr;
        errno = 0;
        quality = std::strtol(s.fields[5].c_str(), &end, 10);
        if (errno || *end || quality < 0) quality = -1;
    }
    fix_.valid = quality > 0;
    if (!s.fields[0].empty()) fix_.utc = s.fields[0];

    if (!fix_.valid) return;   /* no-fix report: don't overwrite stale-but-last-known fields */

    double lat = 0.0, lon = 0.0, alt = 0.0, hdop = 0.0;
    bool have_lat = parseLatLon(s.fields[1], s.fields[2], 2, &lat);
    bool have_lon = parseLatLon(s.fields[3], s.fields[4], 3, &lon);
    bool have_alt = parseDouble(s.fields[8], &alt);
    bool have_hdop = s.fields[7].empty() ? false : parseDouble(s.fields[7], &hdop);
    if (!have_lat || !have_lon) return;   /* claims a fix but the position is unparsable: don't trust it */

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
    if (!s.fields[0].empty()) fix_.utc = s.fields[0];

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
    if (!have_lat || !have_lon) return;

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

}  // namespace model
