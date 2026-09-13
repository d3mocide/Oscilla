/*
 * deauth_model.h — the deck's view of deauth_detector (OCP-SPEC §10.5): a
 * live log of deauth/disassoc detections. Framework-agnostic; host-tested
 * in test/host/deauth_model_test.cpp.
 *
 * There is no snapshot-dump verb for deauth_detector (none registered in
 * ocp.h): the [EVT] kind=deauth stream is the whole record, so this model
 * just keeps the most recent rows rather than reconciling against a dump —
 * same shape as SpectrumModel, for the same reason.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct DeauthEvent {
    std::string bssid;
    std::string mac;
    long reason = 0;
    bool disassoc = false;
    int rssi = 0;
    uint8_t ch = 0;
};

class DeauthModel {
public:
    static constexpr size_t kMaxRows = 64;   /* newest-first, oldest dropped past this */

    /* deauth_detector (re)started: forget the old session's log. */
    void begin();

    /* [STOP] landed: the log persists for a last look, only "live" stops. */
    void stop() { active_ = false; }

    /* Probe rebooted: nothing here is trustworthy any more. */
    void clear();

    /* Absorb an [EVT] kind=deauth. */
    void absorbEvent(const ocp::Item &evt);

    bool active() const { return active_; }
    uint32_t totalCount() const { return total_; }
    const std::vector<DeauthEvent> &events() const { return events_; }

private:
    bool active_ = false;
    uint32_t total_ = 0;
    std::vector<DeauthEvent> events_;   /* index 0 = most recent */
};

}  // namespace model
