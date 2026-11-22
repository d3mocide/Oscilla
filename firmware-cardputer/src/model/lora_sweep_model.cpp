/*
 * lora_sweep_model.cpp — see lora_sweep_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/lora_sweep_model.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <string>

namespace model {

namespace {

bool parseLongRange(const std::string &s, long lo, long hi, long *out)
{
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno || !end || *end != '\0' || v < lo || v > hi) return false;
    *out = v;
    return true;
}

}  // namespace

void LoraSweepModel::begin(uint32_t start_khz, uint16_t step_khz, uint8_t bins)
{
    const bool span_changed = (start_khz != start_khz_ || step_khz != step_khz_ || bins != bins_);
    start_khz_ = start_khz;
    step_khz_ = step_khz;
    bins_ = bins;
    active_ = true;
    if (span_changed) {
        current_.fill(-127);
        peak_.fill(-127);
        history_head_ = 0;
        history_count_ = 0;
        peak_bin_ = 0;
        peak_dbm_ = -127;
        floor_dbm_ = -127;
        sweeps_ = 0;
    }
}

void LoraSweepModel::stop()
{
    active_ = false;
}

void LoraSweepModel::clear()
{
    active_ = false;
    bins_ = 0;
    current_.fill(-127);
    peak_.fill(-127);
    history_head_ = 0;
    history_count_ = 0;
    peak_bin_ = 0;
    peak_dbm_ = -127;
    floor_dbm_ = -127;
    sweeps_ = 0;
    malformed_ = 0;
    last_ms_ = 0;
}

void LoraSweepModel::clearPeak()
{
    if (sweeps_ > 0 && bins_ > 0) {
        peak_ = current_;
        peak_bin_ = 0;
        peak_dbm_ = current_[0];
        for (size_t i = 1; i < bins_; ++i) {
            if (current_[i] > peak_dbm_) {
                peak_dbm_ = current_[i];
                peak_bin_ = i;
            }
        }
    } else {
        peak_.fill(-127);
        peak_bin_ = 0;
        peak_dbm_ = -127;
    }
}

int8_t LoraSweepModel::historyDbm(size_t age_row, size_t bin) const
{
    if (age_row >= history_count_ || bin >= bins_) return -127;
    const size_t idx = (history_head_ + kHistoryRows - 1u - age_row) % kHistoryRows;
    return history_[idx][bin];
}

bool LoraSweepModel::absorbEvent(const ocp::Item &evt, uint32_t now_ms)
{
    if (evt.kind != ocp::ItemKind::Event) return false;
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_LORA_SWEEP) return false;

    const auto *s_start = evt.get(OCP_K_START_KHZ);
    const auto *s_step = evt.get(OCP_K_STEP_KHZ);
    const auto *s_bins = evt.get(OCP_K_BINS);
    const auto *s_rssi = evt.get(OCP_K_RSSI);
    long start_khz = 0, step_khz = 0, bins = 0;
    if (!s_start || !s_step || !s_bins || !s_rssi ||
        !parseLongRange(*s_start, 150000, 960000, &start_khz) ||
        !parseLongRange(*s_step, 50, 2000, &step_khz) ||
        !parseLongRange(*s_bins, 8, static_cast<long>(kMaxBins), &bins)) {
        ++malformed_;
        return false;
    }
    const uint64_t end_khz = static_cast<uint64_t>(start_khz) +
                             static_cast<uint64_t>(bins - 1) * static_cast<uint64_t>(step_khz);
    if (end_khz > 960000ULL) {
        ++malformed_;
        return false;
    }

    std::array<int8_t, kMaxBins> parsed{};
    parsed.fill(-127);
    size_t count = 0;
    size_t pos = 0;
    while (pos <= s_rssi->size()) {
        size_t comma = s_rssi->find(',', pos);
        if (comma == std::string::npos) comma = s_rssi->size();
        if (count >= static_cast<size_t>(bins)) {
            ++malformed_;
            return false;
        }
        long v = 0;
        if (!parseLongRange(s_rssi->substr(pos, comma - pos), -127, 0, &v)) {
            ++malformed_;
            return false;
        }
        parsed[count++] = static_cast<int8_t>(v);
        if (comma == s_rssi->size()) break;
        pos = comma + 1;
    }
    if (count != static_cast<size_t>(bins)) {
        ++malformed_;
        return false;
    }

    const bool span_changed = (static_cast<uint32_t>(start_khz) != start_khz_ ||
                               static_cast<uint16_t>(step_khz) != step_khz_ ||
                               static_cast<uint8_t>(bins) != bins_ ||
                               sweeps_ == 0);
    start_khz_ = static_cast<uint32_t>(start_khz);
    step_khz_ = static_cast<uint16_t>(step_khz);
    bins_ = static_cast<uint8_t>(bins);
    current_ = parsed;

    if (span_changed) {
        peak_ = parsed;
        history_head_ = 0;
        history_count_ = 0;
    } else {
        for (size_t i = 0; i < bins_; ++i) {
            if (current_[i] > peak_[i]) peak_[i] = current_[i];
        }
    }

    history_[history_head_] = parsed;
    history_head_ = (history_head_ + 1u) % kHistoryRows;
    if (history_count_ < kHistoryRows) ++history_count_;

    /* Peak bin from peak-hold envelope; noise floor = 25th percentile of current sweep. */
    peak_bin_ = 0;
    peak_dbm_ = peak_[0];
    std::array<int8_t, kMaxBins> sorted = current_;
    std::sort(sorted.begin(), sorted.begin() + bins_);
    floor_dbm_ = sorted[bins_ / 4];
    for (size_t i = 1; i < bins_; ++i) {
        if (peak_[i] > peak_dbm_) {
            peak_dbm_ = peak_[i];
            peak_bin_ = i;
        }
    }

    ++sweeps_;
    last_ms_ = now_ms;
    return true;
}

}  // namespace model
