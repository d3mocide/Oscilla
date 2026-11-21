/* Session identity and record parsing; storage reads stay outside this model. */
#include "model/session_review.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace model {
namespace {
bool number(const std::string &s, long long low, long long high, long long &out) {
    if (s.empty() || s.size() > 20) return false;
    size_t start = s[0] == '-' ? 1 : 0;
    if (start == s.size()) return false;
    for (size_t i = start; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    char *end = nullptr;
    out = std::strtoll(s.c_str(), &end, 10);
    return *end == 0 && out >= low && out <= high;
}
bool real(const std::string &s, double low, double high) {
    char *end = nullptr;
    double n = std::strtod(s.c_str(), &end);
    return !s.empty() && *end == 0 && std::isfinite(n) && n >= low && n <= high;
}
std::vector<std::string> fields(const std::string &line) {
    std::vector<std::string> out;
    size_t begin = 0;
    for (size_t i = 0; i <= line.size(); ++i)
        if (i == line.size() || line[i] == ',') {
            out.push_back(line.substr(begin, i - begin));
            begin = i + 1;
            if (out.size() > 16) return {};
        }
    return out;
}
bool mac(const std::string &s) {
    if (s.size() != 17) return false;
    for (size_t i = 0; i < 17; ++i)
        if (i % 3 == 2 ? s[i] != ':' : hexNibble(s[i]) < 0) return false;
    return true;
}
std::string unmunge(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '3' && s[i + 2] >= '0' &&
            s[i + 2] <= '7' && s[i + 3] >= '0' && s[i + 3] <= '7') {
            out += char((s[i + 1] - '0') * 64 + (s[i + 2] - '0') * 8 + s[i + 3] - '0');
            i += 3;
        } else
            out += s[i];
    }
    return out;
}
bool utc(const std::string &s, uint64_t &time) {
    if (s.size() != 19) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        char delimiter = i == 4 || i == 7 ? '-' : i == 10 ? ' ' : i == 13 || i == 16 ? ':' : 0;
        if (delimiter ? s[i] != delimiter : s[i] < '0' || s[i] > '9') return false;
    }
    int y = std::atoi(s.substr(0, 4).c_str()), m = std::atoi(s.substr(5, 2).c_str()),
        d = std::atoi(s.substr(8, 2).c_str());
    int h = std::atoi(s.substr(11, 2).c_str()), n = std::atoi(s.substr(14, 2).c_str()),
        sec = std::atoi(s.substr(17, 2).c_str());
    if (y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 || h > 23 || n > 59 || sec > 59) return false;
    const int days[] = {31, 28 + int(y % 4 == 0), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (d > days[m - 1]) return false;
    uint64_t count = 0;
    for (int year = 2000; year < y; ++year) count += 365 + (year % 4 == 0);
    for (int month = 1; month < m; ++month) count += days[month - 1];
    time = (((count + d - 1) * 24 + h) * 60 + n) * 60000 + sec * 1000;
    return true;
}
bool parseRecord(SessionKind kind, const std::string &line, SavedRecord &r) {
    auto f = fields(line);
    long long v = 0;
    if (kind == SessionKind::Lora) {
        if (f.size() != 10 || !number(f[0], 0, UINT32_MAX, v)) return false;
        r.packet.received_ms = static_cast<uint32_t>(v);
        r.time = v;
        r.timed = true;
        r.stamp = f[0] + "ms";
        if (!number(f[1], 1, UINT32_MAX, v)) return false;
        r.frequency = v;
        if (!number(f[2], 5, 12, v)) return false;
        r.sf = v;
        if (!number(f[3], 1, 1000, v)) return false;
        r.bw = v;
        if (!number(f[4], 1, 4, v)) return false;
        r.cr = v;
        if (!number(f[5], -200, 0, v) || !real(f[6], -128, 127)) return false;
        r.rssi = v;
        r.packet.rssi = v;
        r.packet.snr = std::strtof(f[6].c_str(), nullptr);
        if (!number(f[7], 0, 255, v) || f[8].size() != size_t(v) * 2) return false;
        for (char c : f[8])
            if (hexNibble(c) < 0) return false;
        r.packet.len = v;
        r.packet.hex = f[8];
        r.packet.framing = classifyLoraFrameHex(r.packet.hex);
        r.name = "RAW PACKET";
        return true;
    }
    if (f.size() != 14 || !mac(f[0]) || f[13] != "WIFI" || f[1].size() > 128) return false;
    r.identity = f[0];
    r.name = unmunge(f[1]);
    r.auth = f[2];
    r.stamp = f[3];
    r.timed = utc(f[3], r.time);
    if (!number(f[4], 1, 196, v)) return false;
    r.ch = v;
    if (!number(f[5], 2000, 6000, v)) return false;
    r.frequency = v;
    if (!number(f[6], -128, 127, v) || !real(f[7], -90, 90) || !real(f[8], -180, 180)) return false;
    r.rssi = v;
    return true;
}
} // namespace
bool sessionName(const std::string &name, SessionKind kind, SessionEntry &e) {
    const size_t prefix = kind == SessionKind::Drive ? 6 : 4;
    if (name.compare(0, prefix, kind == SessionKind::Drive ? "drive_" : "log_") || name.size() < prefix + 8)
        return false;
    long long index;
    if (!number(name.substr(prefix, 4), 1, 9999, index)) return false;
    auto suffix = name.substr(prefix + 4);
    unsigned parts = suffix == ".csv"                                       ? Csv
                     : kind == SessionKind::Drive && suffix == ".kml"       ? Kml
                     : kind == SessionKind::Lora && suffix == ".meta"       ? Manifest
                     : kind == SessionKind::Lora && suffix == ".health.csv" ? Health
                                                                            : 0;
    if (!parts) return false;
    e = {kind, int(index), parts, 0};
    return true;
}
std::string sessionStem(const SessionEntry &e) {
    char s[20];
    std::snprintf(s, sizeof s, "%s_%04d", e.kind == SessionKind::Drive ? "drive" : "log", e.index);
    return s;
}
void SessionCatalog::add(const SessionEntry &e) {
    for (auto &row : entries)
        if (row.kind == e.kind && row.index == e.index) {
            row.parts |= e.parts;
            row.bytes += e.bytes;
            return;
        }
    entries.push_back(e);
    std::sort(entries.begin(), entries.end(), [](const SessionEntry &a, const SessionEntry &b) {
        return a.kind == b.kind ? a.index > b.index : a.kind < b.kind;
    });
    if (entries.size() > 64) {
        entries.pop_back();
        overflow = true;
    }
}
void SessionSummary::add(const SavedRecord &r) {
    if (!valid) {
        first = r.time;
        first_stamp = r.stamp;
    }
    if (r.timed) {
        if (timed && r.time < last) time_order = false;
        ++timed;
        last = r.time;
        last_stamp = r.stamp;
    }
    ++valid;
    if (signal_count == signals.size())
        std::move(signals.begin() + 1, signals.end(), signals.begin());
    else
        ++signal_count;
    signals[signal_count - 1] = r.rssi;
}
void SessionParser::begin(SessionKind kind, size_t page_index) {
    kind_ = kind;
    page_index_ = page_index;
    line_number_ = 0;
    line_.clear();
    overlong_ = false;
    summary = {};
    page.clear();
}
void SessionParser::feed(char byte) {
    if (byte == '\n') {
        if (overlong_) {
            ++summary.bad;
            ++line_number_;
        } else
            line();
        line_.clear();
        overlong_ = false;
        return;
    }
    if (line_.size() < 768)
        line_ += byte;
    else
        overlong_ = true;
}
void SessionParser::finish() {
    summary.partial = overlong_ || !line_.empty();
    line_.clear();
    overlong_ = false;
}
void SessionParser::line() {
    ++line_number_;
    if (!line_.empty() && line_.back() == '\r') line_.pop_back();
    if (kind_ == SessionKind::Lora && line_number_ == 1) {
        summary.header = line_ == "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing";
        return;
    }
    if (kind_ == SessionKind::Drive && line_number_ == 1) {
        summary.header = line_.compare(0, 14, "WigleWifi-1.6,") == 0;
        return;
    }
    if (kind_ == SessionKind::Drive && line_number_ == 2) {
        summary.header =
            summary.header && line_ == "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
                                       "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";
        return;
    }
    if (!summary.header) return;
    SavedRecord row;
    if (!parseRecord(kind_, line_, row)) {
        ++summary.bad;
        return;
    }
    if (summary.valid / 6 == page_index_) page.push_back(row);
    summary.add(row);
}
} // namespace model
