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
#include "model/number_parse.h"
#include "model/snapshot_rows.h"

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
    row.tracker_kind = f[3];
    row.tracker = !f[3].empty();
    row.rssi = static_cast<int>(rssi);
    uint64_t n = 0;
    if (!parseUnsigned(f[5], &n) || n > UINT32_MAX) return false;
    row.n = static_cast<uint32_t>(n);
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

void BtModel::beginContinuous()
{
    devices_.clear();
    devices_.shrink_to_fit();
    malformed_ = 0;
    continuous_active_ = true;
}

void BtModel::beginAirtag()
{
    tracker_hits_.clear();
    tracker_hits_.shrink_to_fit();
    tracker_total_ = 0;
    airtag_active_ = true;
}

void BtModel::resetTrackerLog()
{
    tracker_hits_.clear();
    tracker_hits_.shrink_to_fit();
    tracker_total_ = 0;
}

void BtModel::stop()
{
    scanning_ = false;
    continuous_active_ = false;
    airtag_active_ = false;
}

void BtModel::clear()
{
    devices_.clear();
    devices_.shrink_to_fit();
    malformed_ = 0;
    scanning_ = false;
    continuous_active_ = false;
    tracker_hits_.clear();
    tracker_hits_.shrink_to_fit();
    tracker_total_ = 0;
    airtag_active_ = false;
}

void BtModel::absorbScan(const ocp::Item &frame,uint32_t now_ms)
{
    if (frame.tag != OCP_MARK_BLE) return;

    const auto bad = replaceSnapshotRows(devices_, frame, kMaxDevices,
        [this, now_ms](const std::string &raw, BtDevice &row) {
            if (!parseBtRow(raw, row)) return false;
            row.observed_ms = now_ms;
            row.observation_id = ++observation_id_;
            return true;
        });
    malformed_ = bad;
    scanning_ = false;   /* [BLE] has no paging: this frame is already the whole result */
}

uint16_t BtModel::deviceTrackerCount() const
{
    uint16_t count = 0;
    for (const auto &device : devices_) {
        if (device.tracker) count++;
    }
    return count;
}

BtDevice *BtModel::findOrInsertDevice(const std::string &mac)
{
    for (auto &d : devices_) {
        if (d.mac == mac) return &d;
    }
    if (devices_.size() >= kMaxDevices) return nullptr;   /* full: dropped, same posture as the probe's own table */
    /* One table-sized block instead of a doubling chain: each doubling held the
     * old and new blocks at once and left holes, which the PSRAM-less deck could
     * not afford (WORKLOG 2026-11-24). begin*() still releases it. */
    if (devices_.size() == devices_.capacity()) devices_.reserve(kMaxDevices);
    devices_.push_back(BtDevice{});
    devices_.back().mac = mac;
    return &devices_.back();
}

const char *trackerShortLabel(const std::string &kind)
{
    if (kind == OCP_TRACKER_AIRTAG)   return "AT ";
    if (kind == OCP_TRACKER_FMDN)     return "GFM";
    if (kind == OCP_TRACKER_SMARTTAG) return "STG";
    if (kind == OCP_TRACKER_TILE)     return "TIL";
    if (kind == OCP_TRACKER_CHIPOLO)  return "CHP";
    return kind.empty() ? "   " : "TRK";
}

void BtModel::absorbEvent(const ocp::Item &evt,uint32_t now_ms)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind) return;

    if (*kind == OCP_EVT_KIND_AIRTAG) {
        const auto *mac = evt.get(OCP_K_MAC);
        const auto *rssi_s = evt.get(OCP_K_RSSI);
        const auto *trk = evt.get(OCP_K_TRACKER);
        long rssi = 0;
        if (rssi_s) toLong(*rssi_s, -128, 127, rssi);

        BtTrackerHit hit;
        hit.mac = mac ? *mac : std::string("?");
        hit.tracker_kind = (trk && !trk->empty()) ? *trk : std::string(OCP_TRACKER_AIRTAG);
        hit.rssi = static_cast<int>(rssi);

        tracker_hits_.insert(tracker_hits_.begin(), std::move(hit));
        if (tracker_hits_.size() > kMaxTrackerHits) tracker_hits_.resize(kMaxTrackerHits);
        tracker_total_++;
    } else if (*kind == OCP_EVT_KIND_BLE) {
        const auto *mac = evt.get(OCP_K_MAC);
        if (!mac || !looksLikeMac(*mac)) return;

        const auto *signal=evt.get(OCP_K_RSSI);long measured=0;
        if(!signal||!toLong(*signal,-128,127,measured)){++malformed_;return;}
        BtDevice *row = findOrInsertDevice(*mac);
        if (!row) return;   /* table full: dropped, not swapped in for an older row */

        if (const auto *name = evt.get(OCP_K_NAME)) row->name = *name;
        if (const auto *mfr = evt.get(OCP_K_MFR)) row->mfr = *mfr;
        if (const auto *tracker = evt.get(OCP_K_TRACKER)) {
            row->tracker_kind = *tracker;
            row->tracker = !tracker->empty();
        }
        row->rssi=static_cast<int>(measured);
        row->observed_ms=now_ms;row->observation_id=++observation_id_;row->first_sighting=true;
        row->n = 1;   /* start_ble_scan only ever reports a first sighting (OCP-SPEC §11.3) */
    }
}

}  // namespace model
