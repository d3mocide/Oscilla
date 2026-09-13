/*
 * spectrum_model.h — the deck's view of channel_view / packet_monitor
 * (OCP-SPEC §10.6). Framework-agnostic; host-tested in
 * test/host/spectrum_model_test.cpp.
 *
 * There is no snapshot-dump verb for either command: [EVT] kind=chan is the
 * only source of truth, and it's self-healing under drops (every channel
 * gets a fresh reading again next cycle), so this model just keeps the most
 * recent reading per channel rather than reconciling against a dump.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct ChannelReading {
    uint8_t ch = 0;
    bool band5 = false;
    uint32_t pkts = 0;
};

class SpectrumModel {
public:
    /* channel_view (re)started: broad mode, one reading per channel. */
    void begin();

    /* packet_monitor <ch> (re)started: locked mode, one channel only. */
    void beginLocked(uint8_t ch);

    /* Probe rebooted: readings are stale by definition. */
    void clear();

    /* [STOP] landed: readings persist for a last look, only "live" stops. */
    void stop() { active_ = false; }

    /* Absorb an [EVT] kind=chan: updates (or, in broad mode, adds) one reading. */
    void absorbEvent(const ocp::Item &evt);

    bool active() const { return active_; }
    bool locked() const { return locked_; }
    uint8_t lockedChannel() const { return locked_ch_; }
    const std::vector<ChannelReading> &readings() const { return readings_; }

private:
    bool active_ = false;
    bool locked_ = false;
    uint8_t locked_ch_ = 0;
    std::vector<ChannelReading> readings_;
};

}  // namespace model
