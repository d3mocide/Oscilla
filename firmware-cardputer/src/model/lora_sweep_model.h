/*
 * lora_sweep_model.h — deck-side passive sub-GHz RSSI spectrum & waterfall
 * model (`lora_sweep`, WP-8). Framework-agnostic, allocation-free fixed arrays;
 * host-tested in test/host/lora_sweep_model_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ocp.h"
#include "ocp/ocp_item.h"

namespace model {

struct LoraSweepPreset {
    const char *label;
    uint32_t start_khz;
    uint16_t step_khz;
    uint8_t  bins;
};

static constexpr LoraSweepPreset kLoraSweepPresets[] = {
    { "US 902-928", 902000u, 400u, 64u },
    { "EU 863-870", 863000u, 100u, 64u },
    { "CELL 869-894", 869000u, 400u, 64u },   /* US 850 cellular downlink, energy only */
};
static constexpr size_t kLoraSweepPresetCount = sizeof(kLoraSweepPresets) / sizeof(kLoraSweepPresets[0]);

class LoraSweepModel {
public:
    static constexpr size_t kMaxBins = OCP_LORA_SWEEP_MAX_BINS;
    static constexpr size_t kHistoryRows = 24;

    void begin(uint32_t start_khz, uint16_t step_khz, uint8_t bins);
    void stop();
    void clear();
    void clearPeak();

    bool absorbEvent(const ocp::Item &evt, uint32_t now_ms);

    bool active() const { return active_; }
    bool hasData() const { return sweeps_ > 0 && bins_ > 0; }
    uint32_t startKhz() const { return start_khz_; }
    uint16_t stepKhz() const { return step_khz_; }
    uint8_t bins() const { return bins_; }
    uint32_t endKhz() const { return bins_ > 0 ? start_khz_ + uint32_t(bins_ - 1u) * step_khz_ : start_khz_; }
    uint32_t binFreqKhz(size_t idx) const { return start_khz_ + uint32_t(idx) * step_khz_; }

    int8_t currentDbm(size_t idx) const { return idx < bins_ ? current_[idx] : int8_t(-127); }
    int8_t peakDbm(size_t idx) const { return idx < bins_ ? peak_[idx] : int8_t(-127); }

    /* History row 0 = newest sweep, row (historyCount()-1) = oldest retained. */
    size_t historyCount() const { return history_count_; }
    int8_t historyDbm(size_t age_row, size_t bin) const;

    size_t peakBin() const { return peak_bin_; }
    int8_t maxPeakDbm() const { return peak_dbm_; }
    int8_t floorDbm() const { return floor_dbm_; }
    uint32_t sweeps() const { return sweeps_; }
    uint32_t malformed() const { return malformed_; }
    uint32_t lastMs() const { return last_ms_; }

private:
    bool active_ = false;
    uint32_t start_khz_ = 902000u;
    uint16_t step_khz_ = 400u;
    uint8_t bins_ = 0;

    std::array<int8_t, kMaxBins> current_{};
    std::array<int8_t, kMaxBins> peak_{};
    std::array<std::array<int8_t, kMaxBins>, kHistoryRows> history_{};
    size_t history_head_ = 0;
    size_t history_count_ = 0;

    size_t peak_bin_ = 0;
    int8_t peak_dbm_ = -127;
    int8_t floor_dbm_ = -127;
    uint32_t sweeps_ = 0;
    uint32_t malformed_ = 0;
    uint32_t last_ms_ = 0;
};

}  // namespace model
