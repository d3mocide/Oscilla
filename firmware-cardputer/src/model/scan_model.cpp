/*
 * scan_model.cpp — see scan_model.h. The probe's rows are validated, never
 * trusted: a malformed row is counted and skipped.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/scan_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"
#include "ocp/ocp_csv.h"
#include "ocp/ocp_parser.h"

namespace model {

namespace {

bool toLong(const std::string &s, long lo, long hi, long &out)
{
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno || *end || v < lo || v > hi) return false;
    out = v;
    return true;
}

long kvLong(const ocp::Item &it, const char *key, long lo, long hi, long dflt)
{
    long v = dflt;
    if (const auto *s = it.get(key)) toLong(*s, lo, hi, v);
    return v;
}

bool parseRow(const std::string &raw, uint16_t expect_idx, ApRow &row)
{
    std::vector<std::string> f;
    if (!ocp::splitCsvRow(raw, f) || f.size() != OCP_SCAN_CSV_FIELDS) return false;

    long idx, ch, rssi;
    if (!toLong(f[0], 1, 65535, idx) || idx != expect_idx) return false;
    if (!toLong(f[3], 1, 196, ch)) return false;
    if (!toLong(f[5], -128, 127, rssi)) return false;
    if (f[6] != OCP_BAND_LABEL_24 && f[6] != OCP_BAND_LABEL_5) return false;
    if (f[2].size() != 17) return false;

    row.idx = static_cast<uint16_t>(idx);
    row.ssid = f[1];
    row.bssid = f[2];
    row.ch = static_cast<uint8_t>(ch);
    row.auth = f[4];
    row.rssi = static_cast<int>(rssi);
    row.band5 = f[6] == OCP_BAND_LABEL_5;
    return true;
}

}  // namespace

void ScanModel::begin()
{
    rows_.clear();
    rows_.shrink_to_fit();
    inspect_ = Inspect();
    scanning_ = true;
    aborted_ = false;
    total_ = 0;
    malformed_ = 0;
    elapsed_ms_ = 0;
}

void ScanModel::clear()
{
    begin();
    scanning_ = false;
}

uint16_t ScanModel::absorbPage(const ocp::Item &frame)
{
    if (!scanning_ || frame.tag != OCP_MARK_SCAN) return 0;

    long first = kvLong(frame, OCP_K_FIRST, 1, 65535, 0);
    uint16_t expected_first = static_cast<uint16_t>(rows_.size() + malformed_ + 1);
    if (first != expected_first) return 0;   /* stale or out of order: ignore */

    if (frame.get(OCP_K_ABORTED)) {
        aborted_ = true;
        scanning_ = false;
        return 0;
    }

    total_ = static_cast<uint16_t>(kvLong(frame, OCP_K_TOTAL, 0, 65535, 0));
    elapsed_ms_ = static_cast<uint32_t>(kvLong(frame, OCP_K_ELAPSED_MS, 0, 86400000L, 0));

    uint16_t idx = static_cast<uint16_t>(first);
    for (const auto &raw : frame.rows) {
        if (rows_.size() >= kMaxRows) break;
        ApRow row;
        if (parseRow(raw, idx, row)) rows_.push_back(std::move(row));
        else malformed_++;
        idx++;
    }

    uint16_t next = static_cast<uint16_t>(rows_.size() + malformed_ + 1);
    if (frame.rows.empty() || next > total_ || rows_.size() >= kMaxRows) {
        scanning_ = false;
        return 0;
    }
    return next;
}

void ScanModel::absorbInspect(const ocp::Item &frame)
{
    if (frame.tag != OCP_MARK_INSPECT) return;
    Inspect in;
    in.valid = true;
    in.aborted = frame.get(OCP_K_ABORTED) != nullptr;
    in.idx = static_cast<uint16_t>(kvLong(frame, OCP_K_IDX, 0, 65535, 0));
    if (const auto *b = frame.get(OCP_K_BSSID)) in.bssid = *b;
    in.ch = static_cast<uint8_t>(kvLong(frame, OCP_K_CH, 0, 196, 0));

    if (!frame.rows.empty()) {
        /* The row is k=v: parse it with the tested parser, as a compact frame. */
        ocp::Parser parser;
        ocp::Item row;
        parser.feedLine(std::string(OCP_MARK_INSPECT) + " " + frame.rows[0] + " " + OCP_KW_END,
                        [&](ocp::Item &&it) { if (it.kind == ocp::ItemKind::Frame) row = std::move(it); });
        in.beacons = static_cast<uint16_t>(kvLong(row, OCP_K_BEACONS, 0, 65535, 0));
        in.rssi = static_cast<int>(kvLong(row, OCP_K_RSSI, -128, 127, 0));
        in.rsn = kvLong(row, OCP_K_RSN, 0, 1, 0) == 1;
        in.mfp_capable = kvLong(row, OCP_K_MFP_CAPABLE, 0, 1, 0) == 1;
        in.mfp_required = kvLong(row, OCP_K_MFP_REQUIRED, 0, 1, 0) == 1;
        in.interval_ms = static_cast<uint16_t>(kvLong(row, OCP_K_INTERVAL_MS, 0, 65535, 0));
        if (const auto *u = row.get(OCP_K_UPTIME_S)) {   /* 64-bit: long is 32-bit on the S3 */
            char *end = nullptr;
            errno = 0;
            unsigned long long v = std::strtoull(u->c_str(), &end, 10);
            if (!errno && !u->empty() && !*end) in.uptime_s = v;
        }
    }
    inspect_ = in;
}

}  // namespace model
