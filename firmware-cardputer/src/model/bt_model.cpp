/*
 * bt_model.cpp — see bt_model.h. The probe's rows are validated, never
 * trusted: a malformed row is counted and skipped (mirrors contacts_model.cpp).
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/bt_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"
#include "ocp/ocp_csv.h"

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

bool looksLikeMac(const std::string &s) { return s.size() == 17; }

bool parseBtRow(const std::string &raw, BtDevice &row)
{
    std::vector<std::string> f;
    if (!ocp::splitCsvRow(raw, f) || f.size() != OCP_BLE_CSV_FIELDS) return false;

    long rssi;
    if (!looksLikeMac(f[0])) return false;
    if (!toLong(f[4], -128, 127, rssi)) return false;

    row.mac = f[0];
    row.name = f[1];
    row.mfr = f[2];
    row.tracker = f[3] == OCP_EVT_KIND_AIRTAG;
    row.rssi = static_cast<int>(rssi);
    row.n = static_cast<uint32_t>(std::strtoul(f[5].c_str(), nullptr, 10));
    return true;
}

}  // namespace

void BtModel::beginScan()
{
    devices_.clear();
    devices_.shrink_to_fit();
    malformed_ = 0;
    scanning_ = true;
}

void BtModel::beginAirtag()
{
    tracker_hits_.clear();
    tracker_hits_.shrink_to_fit();
    tracker_total_ = 0;
    airtag_active_ = true;
}

void BtModel::stop()
{
    scanning_ = false;
    airtag_active_ = false;
}

void BtModel::clear()
{
    devices_.clear();
    devices_.shrink_to_fit();
    malformed_ = 0;
    scanning_ = false;
    tracker_hits_.clear();
    tracker_hits_.shrink_to_fit();
    tracker_total_ = 0;
    airtag_active_ = false;
}

void BtModel::absorbScan(const ocp::Item &frame)
{
    if (frame.tag != OCP_MARK_BLE) return;

    std::vector<BtDevice> rows;
    uint16_t bad = 0;
    for (const auto &raw : frame.rows) {
        if (rows.size() >= kMaxDevices) break;
        BtDevice row;
        if (parseBtRow(raw, row)) rows.push_back(std::move(row));
        else bad++;
    }
    devices_ = std::move(rows);
    malformed_ = bad;
    scanning_ = false;   /* [BLE] has no paging: this frame is already the whole result */
}

void BtModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_AIRTAG) return;

    const auto *mac = evt.get(OCP_K_MAC);
    const auto *rssi_s = evt.get(OCP_K_RSSI);
    long rssi = 0;
    if (rssi_s) toLong(*rssi_s, -128, 127, rssi);

    BtTrackerHit hit;
    hit.mac = mac ? *mac : std::string("?");
    hit.rssi = static_cast<int>(rssi);

    tracker_hits_.insert(tracker_hits_.begin(), std::move(hit));
    if (tracker_hits_.size() > kMaxTrackerHits) tracker_hits_.resize(kMaxTrackerHits);
    tracker_total_++;
}

}  // namespace model
