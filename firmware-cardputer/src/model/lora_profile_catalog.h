/*
 * lora_profile_catalog.h — built-in + optional SD-loaded LoRa RX presets
 * (WP-9a). Framework-agnostic pure C++; host-tested in
 * test/host/lora_profile_catalog_test.cpp.
 *
 * CSV format (/oscilla/lora_profiles.csv):
 *   token,label,freq_hz,sf,bw_khz,cr,sync_word
 * Comments starting with '#' and blank lines are ignored. Every field is
 * strictly validated against SX1262 receive ranges; invalid rows are rejected
 * without disturbing built-in defaults.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "model/lora_profiles.h"

namespace model {

struct LoraProfile {
    std::string token;
    std::string label;
    uint32_t freq_hz = 0;
    int sf = 0;
    int bw_khz = 0;
    int cr = 0;
    uint8_t sync_word = 0x12;
    bool from_sd = false;
};

class LoraProfileCatalog {
public:
    static constexpr size_t kMaxProfiles = 16;

    LoraProfileCatalog() { resetToBuiltins(); }

    void resetToBuiltins();
    /* Appends valid profiles from CSV text up to kMaxProfiles. Returns number
     * of SD profiles accepted. */
    size_t parseCsv(const std::string &csv_text);

    size_t size() const { return profiles_.size(); }
    const LoraProfile &at(size_t index) const { return profiles_[index < profiles_.size() ? index : 0]; }
    size_t sdCount() const { return sd_count_; }
    size_t rejectedRows() const { return rejected_; }

    /* Returns matching profile index, or 0 (first built-in) if absent. */
    size_t findByToken(const std::string &token) const;
    /* Human label for a wire/manifest token, or "MANUAL" if unrecognized. */
    std::string labelForToken(const std::string &token) const;

    static bool isValidBwKhz(int bw_khz);

private:
    std::vector<LoraProfile> profiles_;
    size_t sd_count_ = 0;
    size_t rejected_ = 0;
};

}  // namespace model
