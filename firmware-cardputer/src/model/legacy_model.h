/*
 * legacy_model.h — the deck's view of legacy_config/legacy_listen/status
 * (CC1101): a live log of received sub-GHz chunks, same shape as
 * model::LoraModel minus the LoRa-only fields (no SF/BW/CR, no SNR, no
 * framing guess — CC1101 v1 has none of those concepts, raw capture only).
 * Framework-agnostic; host-tested in test/host/legacy_model_test.cpp.
 *
 * Called "chunk," not "packet," on purpose: with no sync word and no CRC,
 * each `[EVT] kind=legacy` is whatever bytes the carrier-sense-gated FIFO
 * happened to hold when the poll ran, not a framed, decoded packet — see
 * cc1101_radio.c's own header. Naming it "packet" anywhere (this type, the
 * screen, the log columns) claims more than the driver actually delivers.
 *
 * Showing/logging the raw payload is a deliberate choice, not a departure
 * from a security rule: SECURITY.md governs what leaves the deck (SD
 * export, sharing) and requires every renderer to re-escape decoded bytes
 * before display (both satisfied here the same way LoraModel/subghz_view
 * already do it — printable() at render time, an opt-in per-session SD
 * logger for export). There is no blanket rule against a CC1101 screen
 * showing what it received.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct LegacyChunk {
    int rssi = 0;
    uint16_t len = 0;
    std::string hex;   /* undecoded payload */
};

class LegacyModel {
public:
    static constexpr size_t kMaxRows = 32;   /* newest-first, oldest dropped past this */

    void configured(uint32_t freq_hz) { has_config_ = true; freq_hz_ = freq_hz; }

    /* legacy_listen (re)started: forget the old session's log. */
    void begin();

    /* [STOP] landed: the log persists for a last look, only "live" stops. */
    void stop() { active_ = false; }

    /* Probe rebooted: nothing here is trustworthy any more. */
    void clear();

    /* Absorb an [EVT] kind=legacy. Returns the new chunk (a pointer into
     * chunks_[0]) on success, or nullptr if the event was a different kind
     * or malformed — callers need this to know when to write a log row
     * without re-parsing the event themselves (same contract as
     * LoraModel::absorbEvent). Malformed events are counted, not silently
     * discarded — see malformedCount(). */
    const LegacyChunk *absorbEvent(const ocp::Item &evt);

    /* [LEGACY] status reply: the probe's overflow=/qdrops= counters for the
     * current session (cc1101_radio.c's FIFO-overflow and radio-queue-drop
     * counts) — echoed here so the deck can show them, not re-derived. */
    void absorbStatus(const ocp::Item &it);

    bool active() const { return active_; }
    bool hasConfig() const { return has_config_; }
    uint32_t freqHz() const { return freq_hz_; }
    uint32_t total() const { return total_; }
    int lastRssi() const { return chunks_.empty() ? 0 : chunks_.front().rssi; }
    const std::vector<LegacyChunk> &chunks() const { return chunks_; }
    /* Events that failed absorbEvent()'s validation this session — a
     * nonzero count means the wire or the probe is misbehaving, not that
     * capture is broken (mirrors ZigModel's malformedRows()). */
    uint32_t malformedCount() const { return malformed_; }
    uint32_t fifoOverflows() const { return fifo_overflows_; }
    uint32_t queueDrops() const { return queue_drops_; }

private:
    bool active_ = false;
    bool has_config_ = false;
    uint32_t freq_hz_ = 0;
    uint32_t total_ = 0;
    uint32_t malformed_ = 0;
    uint32_t fifo_overflows_ = 0;
    uint32_t queue_drops_ = 0;
    std::vector<LegacyChunk> chunks_;   /* index 0 = most recent */
};

}  // namespace model
