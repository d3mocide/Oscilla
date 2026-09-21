/* zig_model.cpp — validates OCP [ZIG] rows before retaining them. */
#include "model/zig_model.h"
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include "ocp.h"
#include "ocp/ocp_csv.h"

namespace model { namespace {
bool number(const std::string &s, long lo, long hi, long &out) { char *e = nullptr; errno = 0; long n = std::strtol(s.c_str(), &e, 10); if (s.empty() || errno || *e || n < lo || n > hi) return false; out = n; return true; }
bool hex4(const std::string &s) { if (s.size() != 4) return false; for (char c : s) if (!std::isxdigit((unsigned char)c)) return false; return true; }
}  // namespace
void ZigModel::begin() { pans_.clear(); nodes_.clear(); malformed_ = 0; active_ = true; }
void ZigModel::stop() { active_ = false; }
void ZigModel::clear() { pans_.clear(); nodes_.clear(); malformed_ = 0; active_ = false; }
void ZigModel::absorb(const ocp::Item &f)
{
    if (f.tag != OCP_MARK_ZIG) return;
    /* Compact start/status replies have no table rows; do not erase a snapshot. */
    if (f.rows.empty()) return;
    std::vector<ZigPan> pans; std::vector<ZigNode> nodes; uint16_t bad = 0;
    for (const auto &raw : f.rows) {
        std::vector<std::string> v; long a, b, c;
        if (!ocp::splitCsvRow(raw, v) || v.size() != 8) { ++bad; continue; }
        if (v[0] == OCP_ZIG_ROW_PAN) {
            if (!hex4(v[1]) || !number(v[5], 0, 96, a) || !number(v[6], -128, 127, b) || !number(v[7], 0, 255, c) || pans.size() == kMaxPans) { ++bad; continue; }
            pans.push_back({v[1], v[2], v[4], (uint16_t)a, (int)b, (uint8_t)c});
        } else if (v[0] == OCP_ZIG_ROW_NODE) {
            if (!hex4(v[1]) || (!v[2].empty() && !hex4(v[2])) || !number(v[5], -128, 127, a) || !number(v[6], 0, 255, b) || !number(v[7], 0, 0x7fffffff, c) || nodes.size() == kMaxNodes) { ++bad; continue; }
            nodes.push_back({v[1], v[2], v[3], v[4], (int)a, (uint8_t)b, (uint32_t)c});
        } else ++bad;
    }
    pans_ = std::move(pans); nodes_ = std::move(nodes); malformed_ = bad;
}
}  // namespace model
