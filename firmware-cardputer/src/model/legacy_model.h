/*
 * legacy_model.h — the deck's view of legacy_config/legacy_listen/status
 * (CC1101 debug-console bring-up only; no screen/view yet — deliberately
 * out of scope for this pass, see WORKLOG). A running count and the most
 * recent RSSI, nothing else: deck_app.h's log()/dump() policy is "counts
 * and states only," and raw captured bytes are field data (SECURITY.md) —
 * this model never stores or logs a payload, unlike LoraModel.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "ocp/ocp_item.h"

namespace model {

class LegacyModel {
public:
    void configured(uint32_t freq_hz) { has_config_ = true; freq_hz_ = freq_hz; }
    void begin() { active_ = true; total_ = 0; last_rssi_ = 0; }
    void stop() { active_ = false; }
    void clear() { active_ = false; has_config_ = false; total_ = 0; last_rssi_ = 0; }

    /* Absorb an [EVT] kind=legacy. Returns true if it was one (and counted),
     * false for any other kind or a malformed event. */
    bool absorbEvent(const ocp::Item &evt);

    bool active() const { return active_; }
    bool hasConfig() const { return has_config_; }
    uint32_t freqHz() const { return freq_hz_; }
    uint32_t total() const { return total_; }
    int lastRssi() const { return last_rssi_; }

private:
    bool active_ = false;
    bool has_config_ = false;
    uint32_t freq_hz_ = 0;
    uint32_t total_ = 0;
    int last_rssi_ = 0;
};

}  // namespace model
