/*
 * lora_model.h — the deck's view of lora_config/lora_listen/lora_status
 * (P3): a live log of received sub-GHz packets. Framework-agnostic;
 * host-tested in test/host/lora_model_test.cpp.
 *
 * There is no snapshot-dump verb for LoRa (none registered in ocp.h): the
 * [EVT] kind=lora stream is the whole record, same shape as SpectrumModel
 * and DeauthModel for the same reason.
 *
 * Configured params come from the deck's own send, not parsed back out of
 * [CFG] — that marker is shared by several verbs (packet_monitor,
 * deauth_detector, lora_config, ...) and isn't decodable from the frame
 * alone, same reasoning deck_app.cpp already documents for OCP_MARK_CFG.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct LoraPacket {
    int rssi = 0;
    float snr = 0.0f;
    uint16_t len = 0;
    std::string hex;   /* undecoded application payload; no framing classifier yet */
};

class LoraModel {
public:
    static constexpr size_t kMaxRows = 32;   /* newest-first, oldest dropped past this */

    /* lora_config accepted: remember what the deck asked for, echoed back
     * from the same values it sent (not parsed from the ambiguous [CFG]). */
    void configured(uint32_t freq_hz, int sf, int bw_khz, int cr);

    /* lora_listen (re)started: forget the old session's log. */
    void begin();

    /* [STOP] landed: the log persists for a last look, only "live" stops. */
    void stop() { active_ = false; }

    /* Probe rebooted: nothing here is trustworthy any more. */
    void clear();

    /* Absorb an [EVT] kind=lora. */
    void absorbEvent(const ocp::Item &evt);

    bool active() const { return active_; }
    bool hasConfig() const { return has_config_; }
    uint32_t freqHz() const { return freq_hz_; }
    int sf() const { return sf_; }
    int bwKhz() const { return bw_khz_; }
    int cr() const { return cr_; }
    uint32_t totalCount() const { return total_; }
    const std::vector<LoraPacket> &packets() const { return packets_; }

private:
    bool active_ = false;
    bool has_config_ = false;
    uint32_t freq_hz_ = 0;
    int sf_ = 0;
    int bw_khz_ = 0;
    int cr_ = 0;
    uint32_t total_ = 0;
    std::vector<LoraPacket> packets_;   /* index 0 = most recent */
};

}  // namespace model
