/*
 * baseline_watch.cpp — see baseline_watch.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/baseline_watch.h"
#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace model {

namespace {

int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool validMac(const std::string &s)
{
    if (s.size() != 17) return false;
    for (size_t i = 0; i < 17; ++i) {
        if (i % 3 == 2 ? s[i] != ':' : hexVal(s[i]) < 0) return false;
    }
    return true;
}

std::string escapeField(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size() + 4);
    static const char hex[] = "0123456789abcdef";
    for (unsigned char c : raw) {
        if (c < 0x20 || c >= 0x7F || c == ',' || c == '\\' || c == '"') {
            out += "\\x";
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::string unescapeField(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size() && s[i + 1] == 'x') {
            const int hi = hexVal(s[i + 2]);
            const int lo = hexVal(s[i + 3]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 3;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::vector<std::string> splitCsv(const std::string &line)
{
    std::vector<std::string> cols;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            cols.push_back(line.substr(start, i - start));
            start = i + 1;
            if (cols.size() > 8) return {};
        }
    }
    return cols;
}

bool parseMac(const std::string &s, uint8_t out[6])
{
    if (!validMac(s)) return false;
    for (int i = 0; i < 6; ++i) out[i] = static_cast<uint8_t>(hexVal(s[i * 3]) << 4 | hexVal(s[i * 3 + 1]));
    return true;
}

/* Every label authRank() ranks, plus the probe's others. Index 0 is "". */
const char *const kAuthLabels[] = {
    "", "OPEN", "WEP", "WPA", "WPA/WPA2", "WPA2", "RSN", "OWE", "WPA2/WPA3", "WPA3",
    "WPA-EAP", "WPA2-EAP", "WPA3-EAP", "WPA2/WPA3-EAP", "WPA3-EAP192", "WAPI", "DPP", "UNKNOWN",
};
constexpr uint8_t kAuthCount = sizeof kAuthLabels / sizeof kAuthLabels[0];

uint8_t authCode(const std::string &auth)
{
    for (uint8_t i = 0; i < kAuthCount; ++i) if (auth == kAuthLabels[i]) return i;
    return kAuthCount - 1;   /* an unlisted label ranks -1 either way */
}

bool parseU8(const std::string &s, uint8_t max_val, uint8_t *out)
{
    if (s.empty() || s.size() > 3) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    unsigned long v = std::strtoul(s.c_str(), nullptr, 10);
    if (v > max_val) return false;
    *out = static_cast<uint8_t>(v);
    return true;
}

}  // namespace

int BaselineWatch::authRank(const std::string &auth)
{
    if (auth.empty() || auth == "OPEN") return 0;
    if (auth == "WEP") return 1;
    if (auth == "WPA" || auth == "WPA/WPA2") return 2;
    if (auth == "WPA2" || auth == "RSN" || auth == "OWE") return 3;
    if (auth == "WPA2/WPA3" || auth == "WPA3" ||
        auth == "WPA-EAP" || auth == "WPA2-EAP" || auth == "WPA3-EAP" ||
        auth == "WPA2/WPA3-EAP" || auth == "WPA3-EAP192") return 4;
    return -1;
}

int BaselineWatch::liveAuthCeiling(const ApRow &row)
{
    if (!row.auth_coarse) return authRank(row.auth);
    if (row.auth == "OPEN") return 0;
    if (row.auth == "WEP") return 2;   /* privacy, no RSN IE: WEP or WPA1 */
    if (row.auth == "RSN") return 4;   /* any RSN mode, up to WPA3/EAP */
    return -1;
}

const char *BaselineWatch::kindLabel(AnomalyKind kind)
{
    switch (kind) {
    case AnomalyKind::NewAp:         return "NEW AP";
    case AnomalyKind::NewPan:        return "NEW 802.15.4 PAN";
    case AnomalyKind::MfpDropped:    return "MFP DROPPED";
    case AnomalyKind::AuthDowngrade: return "AUTH DOWNGRADE";
    case AnomalyKind::EvilTwin:      return "EVIL TWIN?";
    }
    return "DELTA";
}

bool BaselineWatch::isHighSeverity(AnomalyKind kind)
{
    return kind == AnomalyKind::EvilTwin ||
           kind == AnomalyKind::AuthDowngrade ||
           kind == AnomalyKind::MfpDropped;
}

std::string BaselineWatch::bssidText(const BaselineAp &ap)
{
    char out[18];
    std::snprintf(out, sizeof out, "%02x:%02x:%02x:%02x:%02x:%02x",
                  ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    return out;
}

const char *BaselineWatch::authLabel(uint8_t code)
{
    return code < kAuthCount ? kAuthLabels[code] : kAuthLabels[kAuthCount - 1];
}

size_t BaselineWatch::apHeapBytes() const
{
    return aps_.capacity() * sizeof(BaselineAp) + (ssid_pool_.capacity() > 15 ? ssid_pool_.capacity() + 1 : 0);
}

bool BaselineWatch::addAp(const std::string &bssid, const std::string &ssid, const std::string &auth,
                          uint8_t ch, bool mfp)
{
    BaselineAp ap;
    if (aps_.size() >= kMaxBaselineAps || !parseMac(bssid, ap.bssid) || findAp(bssid)) return false;
    const size_t len = std::min<size_t>(ssid.size(), 32);   /* 802.11 SSIDs are at most 32 bytes */
    ap.ch = ch;
    ap.auth = authCode(auth);
    ap.mfp_required = mfp;
    ap.ssid_len = static_cast<uint8_t>(len);
    ap.ssid_off = static_cast<uint16_t>(ssid_pool_.size());   /* <= 256 x 32 bytes */
    ssid_pool_.append(ssid, 0, len);
    aps_.push_back(ap);
    return true;
}

void BaselineWatch::clear()
{
    /* Release, not just empty: the baseline is held for the whole session on a
     * PSRAM-less deck, so its storage is sized once to what it holds. */
    std::vector<BaselineAp>().swap(aps_);
    std::string().swap(ssid_pool_);
    std::vector<BaselinePan>().swap(pans_);
    std::vector<BaselineAnomaly>().swap(anomalies_);
}

void BaselineWatch::captureFrom(const ScanModel &scan, const ZigModel &zig)
{
    clear();
    aps_.reserve(std::min(scan.rows().size(), kMaxBaselineAps));
    pans_.reserve(std::min(zig.pans().size(), kMaxBaselinePans));
    size_t pool = 0;
    for (const auto &r : scan.rows()) pool += std::min<size_t>(r.ssid.size(), 32);
    ssid_pool_.reserve(std::min(pool, kMaxBaselineAps * 32));
    for (const auto &r : scan.rows()) {
        if (aps_.size() >= kMaxBaselineAps) break;
        addAp(r.bssid, r.ssid, r.auth, r.ch, r.beacon_info && r.mfp_required);
    }
    for (const auto &p : zig.pans()) {
        if (pans_.size() >= kMaxBaselinePans) break;
        if (p.pan.empty() || hasPanInBaseline(p.pan)) continue;
        pans_.push_back({p.pan, p.channels});
    }
    aps_.shrink_to_fit();   /* duplicates and bad BSSIDs were skipped */
    ssid_pool_.shrink_to_fit();
    pans_.shrink_to_fit();
}

std::string BaselineWatch::serializeCsv() const
{
    std::string out = "schema=oscilla-baseline-v1\n";
    for (const auto &a : aps_) {
        char tail[24];
        std::snprintf(tail, sizeof tail, ",%u,%u\n",
                      static_cast<unsigned>(a.ch), a.mfp_required ? 1U : 0U);
        out += "AP," + bssidText(a) + "," + escapeField(std::string(ssidOf(a))) + "," +
               escapeField(authLabel(a.auth)) + tail;
    }
    for (const auto &p : pans_) {
        out += "PAN," + escapeField(p.pan_id) + "," + escapeField(p.channels) + "\n";
    }
    return out;
}

bool BaselineWatch::parseCsv(const std::string &csv)
{
    clear();
    size_t ap_lines = csv.compare(0, 3, "AP,") == 0 ? 1 : 0, pan_lines = 0;
    for (size_t at = csv.find('\n'); at != std::string::npos; at = csv.find('\n', at + 1)) {
        if (csv.compare(at + 1, 3, "AP,") == 0) ++ap_lines;
        else if (csv.compare(at + 1, 4, "PAN,") == 0) ++pan_lines;
    }
    aps_.reserve(std::min(ap_lines, kMaxBaselineAps));
    pans_.reserve(std::min(pan_lines, kMaxBaselinePans));
    size_t pos = 0;
    bool header_ok = false;
    while (pos < csv.size()) {
        size_t nl = csv.find('\n', pos);
        if (nl == std::string::npos) nl = csv.size();
        const size_t raw_len = nl - pos;
        std::string line = raw_len <= 256 ? csv.substr(pos, raw_len) : std::string();
        pos = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (!header_ok) {
            if (line != "schema=oscilla-baseline-v1") return false;
            header_ok = true;
            continue;
        }
        auto f = splitCsv(line);
        if (f.size() == 6 && f[0] == "AP" && aps_.size() < kMaxBaselineAps) {
            uint8_t ch = 0, mfp = 0;
            if (!parseU8(f[4], 196, &ch) || !parseU8(f[5], 1, &mfp)) continue;
            addAp(f[1], unescapeField(f[2]), unescapeField(f[3]), ch, mfp != 0);
        } else if (f.size() == 3 && f[0] == "PAN" && pans_.size() < kMaxBaselinePans) {
            std::string pan = unescapeField(f[1]);
            std::string channels = unescapeField(f[2]);
            if (pan.empty() || pan.size() > 16 || channels.size() > 32) continue;
            if (hasPanInBaseline(pan)) continue;
            pans_.push_back({pan, channels});
        }
    }
    aps_.shrink_to_fit();   /* rejected or duplicate rows were reserved for */
    ssid_pool_.shrink_to_fit();
    pans_.shrink_to_fit();
    return header_ok;
}

const BaselineAp *BaselineWatch::findAp(const std::string &bssid) const
{
    uint8_t mac[6];
    if (!parseMac(bssid, mac)) return nullptr;
    for (const auto &a : aps_) if (std::memcmp(a.bssid, mac, sizeof mac) == 0) return &a;
    return nullptr;
}

bool BaselineWatch::hasSsidInBaseline(const std::string &ssid) const
{
    if (ssid.empty()) return false;
    for (const auto &a : aps_) if (ssidOf(a) == ssid) return true;
    return false;
}

bool BaselineWatch::hasPanInBaseline(const std::string &pan_id) const
{
    for (const auto &p : pans_) if (p.pan_id == pan_id) return true;
    return false;
}

void BaselineWatch::upsertAnomaly(AnomalyKind kind, const std::string &id,
                                  const std::string &detail, uint32_t now_ms)
{
    for (auto &a : anomalies_) {
        if (a.id == id) {
            /* Keep higher-severity anomaly when one BSSID triggers multiple deltas. */
            if (static_cast<uint8_t>(kind) >= static_cast<uint8_t>(a.kind)) {
                a.kind = kind;
                a.detail = detail;
            }
            a.seen_ms = now_ms;
            return;
        }
    }
    if (anomalies_.size() < kMaxAnomalies) {
        anomalies_.push_back({kind, id, detail, now_ms});
    } else if (isHighSeverity(kind)) {
        /* Evict the first low-severity entry so a high-severity alert is never lost. */
        for (auto &a : anomalies_) {
            if (!isHighSeverity(a.kind)) {
                a = {kind, id, detail, now_ms};
                return;
            }
        }
    }
}

void BaselineWatch::evaluate(const ScanModel &scan, const ZigModel &zig, uint32_t now_ms)
{
    if (!active()) return;
    for (const auto &r : scan.rows()) {
        if (const auto *base = findAp(r.bssid)) {
            const int base_rank = authRank(authLabel(base->auth));
            const int live_rank = liveAuthCeiling(r);
            if (base_rank >= 0 && live_rank >= 0 && live_rank < base_rank) {
                upsertAnomaly(AnomalyKind::AuthDowngrade, r.bssid, authLabel(base->auth) + ("->" + r.auth), now_ms);
            }
            if (base->mfp_required && r.beacon_info && !r.mfp_required) {
                upsertAnomaly(AnomalyKind::MfpDropped, r.bssid, r.ssid, now_ms);
            }
        } else {
            if (hasSsidInBaseline(r.ssid)) {
                upsertAnomaly(AnomalyKind::EvilTwin, r.bssid, r.ssid, now_ms);
            } else {
                upsertAnomaly(AnomalyKind::NewAp, r.bssid, r.ssid, now_ms);
            }
        }
    }
    for (const auto &p : zig.pans()) {
        if (!p.pan.empty() && !hasPanInBaseline(p.pan)) {
            upsertAnomaly(AnomalyKind::NewPan, p.pan, p.channels.empty() ? "pan" : ("ch" + p.channels), now_ms);
        }
    }
}

void BaselineWatch::evaluateInspect(const Inspect &in, uint32_t now_ms)
{
    if (!active() || !in.valid || !in.beacons) return;
    const auto *base = findAp(in.bssid);
    if (!base) return;
    if (base->mfp_required && !in.mfp_required) {
        upsertAnomaly(AnomalyKind::MfpDropped, in.bssid, std::string(ssidOf(*base)), now_ms);
    }
}

const BaselineAnomaly *BaselineWatch::anomalyForBssid(const std::string &bssid) const
{
    for (const auto &a : anomalies_) {
        if (a.id == bssid) return &a;
    }
    return nullptr;
}

size_t BaselineWatch::highSeverityCount() const
{
    size_t n = 0;
    for (const auto &a : anomalies_) if (isHighSeverity(a.kind)) ++n;
    return n;
}

size_t BaselineWatch::countOf(AnomalyKind kind) const
{
    size_t n = 0;
    for (const auto &a : anomalies_) if (a.kind == kind) ++n;
    return n;
}

}  // namespace model
