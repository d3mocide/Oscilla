/*
 * lora_profile_catalog.cpp — see lora_profile_catalog.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/lora_profile_catalog.h"

#include <cstdlib>

namespace model {

namespace {

std::string trim(const std::string &s)
{
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool validToken(const std::string &t)
{
    if (t.empty() || t.size() > 24) return false;
    for (unsigned char c : t) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

bool validLabel(const std::string &l)
{
    if (l.empty() || l.size() > 14) return false;
    for (unsigned char c : l) {
        if (c < 0x20 || c > 0x7E || c == ',' || c == '"') return false;
    }
    return true;
}

bool parseU32(const std::string &s, uint32_t low, uint32_t high, uint32_t *out)
{
    if (s.empty() || s.size() > 10) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    char *end = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &end, 10);
    if (!end || *end != '\0' || v < low || v > high) return false;
    *out = static_cast<uint32_t>(v);
    return true;
}

bool parseSyncByte(const std::string &s, uint8_t *out)
{
    if (s.empty() || s.size() > 6) return false;
    int base = 10;
    size_t start = 0;
    if (s.size() >= 3 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        start = 2;
    }
    if (start == s.size()) return false;
    for (size_t i = start; i < s.size(); ++i) {
        const char c = s[i];
        const bool dec = c >= '0' && c <= '9';
        const bool hex = (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (base == 10 ? !dec : (!dec && !hex)) return false;
    }
    char *end = nullptr;
    unsigned long v = std::strtoul(s.c_str() + start, &end, base);
    if (!end || *end != '\0' || v > 255UL) return false;
    *out = static_cast<uint8_t>(v);
    return true;
}

bool split7(const std::string &line, std::string out[7])
{
    size_t pos = 0;
    for (int i = 0; i < 7; ++i) {
        const size_t comma = line.find(',', pos);
        if (i < 6) {
            if (comma == std::string::npos) return false;
            out[i] = trim(line.substr(pos, comma - pos));
            pos = comma + 1;
        } else {
            if (comma != std::string::npos) return false;
            out[i] = trim(line.substr(pos));
        }
    }
    return true;
}

}  // namespace

bool LoraProfileCatalog::isValidBwKhz(int bw_khz)
{
    switch (bw_khz) {
    case 7:
    case 10:
    case 15:
    case 20:
    case 31:
    case 41:
    case 62:
    case 125:
    case 250:
    case 500:
        return true;
    default:
        return false;
    }
}

void LoraProfileCatalog::resetToBuiltins()
{
    profiles_.clear();
    sd_count_ = 0;
    rejected_ = 0;
    for (size_t i = 0; i < kLoraProfileCount && profiles_.size() < kMaxProfiles; ++i) {
        const auto &d = kLoraProfiles[i];
        profiles_.push_back({d.token, d.label, d.freq_hz, d.sf, d.bw_khz, d.cr, d.sync_word, false});
    }
}

size_t LoraProfileCatalog::parseCsv(const std::string &csv_text)
{
    size_t added = 0;
    size_t pos = 0;
    bool first_data_line = true;
    while (pos <= csv_text.size()) {
        size_t nl = csv_text.find('\n', pos);
        if (nl == std::string::npos) nl = csv_text.size();
        const size_t raw_len = nl - pos;
        if (raw_len > 256) {
            ++rejected_;
            if (nl == csv_text.size()) break;
            pos = nl + 1;
            continue;
        }
        std::string line = trim(csv_text.substr(pos, raw_len));
        pos = nl + 1;
        if (line.empty() || line[0] == '#') continue;

        if (first_data_line) {
            first_data_line = false;
            if (line.compare(0, 6, "token,") == 0) continue;
        }

        std::string f[7];
        uint32_t freq = 0, sf = 0, bw = 0, cr = 0;
        uint8_t sync = 0;
        if (!split7(line, f) || !validToken(f[0]) || !validLabel(f[1]) ||
            !parseU32(f[2], 150000000U, 960000000U, &freq) ||
            !parseU32(f[3], 5U, 12U, &sf) ||
            !parseU32(f[4], 7U, 500U, &bw) || !isValidBwKhz(static_cast<int>(bw)) ||
            !parseU32(f[5], 1U, 4U, &cr) ||
            !parseSyncByte(f[6], &sync)) {
            ++rejected_;
            continue;
        }

        bool duplicate = false;
        for (const auto &existing : profiles_) {
            if (existing.token == f[0]) { duplicate = true; break; }
        }
        if (duplicate || profiles_.size() >= kMaxProfiles) {
            ++rejected_;
            continue;
        }

        profiles_.push_back({f[0], f[1], freq, static_cast<int>(sf), static_cast<int>(bw),
                             static_cast<int>(cr), sync, true});
        ++added;
        ++sd_count_;
    }
    return added;
}

size_t LoraProfileCatalog::findByToken(const std::string &token) const
{
    for (size_t i = 0; i < profiles_.size(); ++i) {
        if (profiles_[i].token == token) return i;
    }
    return 0;
}

std::string LoraProfileCatalog::labelForToken(const std::string &token) const
{
    for (const auto &p : profiles_) {
        if (p.token == token) return p.label;
    }
    return "MANUAL";
}

}  // namespace model
