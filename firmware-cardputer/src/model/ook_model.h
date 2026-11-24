/*
 * ook_model.h — the deck's view of ook_listen / ook_status (OCP-SPEC §15,
 * D-20): decoded 433 MHz OOK transmissions from the probe's CC1101.
 * Framework-agnostic; host-tested in test/host/ook_model_test.cpp.
 *
 * The probe already merges repeats (one button press, one sensor reading =
 * one kind=ook event), so the deck keeps two views of the stream:
 *  - devices(): one row per transmitter identity (a Princeton code, or a
 *    LaCrosse id + channel), newest activity first, with its last reading;
 *  - packets(): the newest-first event log.
 * kind=ook_unknown summaries only advance a counter and keep the latest
 * shape; the probe sends no raw timings and neither does the deck.
 *
 * Every event is validated whole before it touches state: a malformed or
 * partial line is ignored, never half-applied.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

enum class OokDecoder : uint8_t { Princeton, LaCrosse };

struct OokPacket {
    uint32_t received_ms = 0;
    OokDecoder decoder = OokDecoder::Princeton;
    uint8_t nbits = 0;
    std::string hex;           /* frame, MSB first, lower case */
    uint16_t frames = 0;
    uint16_t te_us = 0;
    bool has_rssi = false;
    int rssi = 0;
    /* LaCrosse TX141THBv2 only */
    uint8_t id = 0;
    uint8_t ch = 0;
    bool batt_low = false;
    bool test = false;
    int temp_c10 = 0;          /* tenths of a degree C */
    uint8_t hum = 0;           /* percent */
};

struct OokDevice {
    OokPacket last;            /* most recent packet from this transmitter */
    uint32_t first_ms = 0;
    uint32_t count = 0;        /* merged events seen this session */
};

struct OokUnknown {
    bool valid = false;
    uint32_t received_ms = 0;
    uint32_t pulses = 0;
    uint32_t min_us = 0;
    uint32_t max_us = 0;
    uint32_t dur_ms = 0;
    bool has_rssi = false;
    int rssi = 0;
};

struct OokHealth {
    bool valid = false;
    uint32_t trains = 0;
    uint32_t packets = 0;
    uint32_t unknown = 0;
    uint32_t overflows = 0;
    uint32_t dropped = 0;
    uint32_t errors = 0;
    uint32_t reinit = 0;
    uint32_t hw_fault = 0;
    uint32_t ocp_drop = 0;
};

/* Transmitter identity: a LaCrosse id + channel, or a Princeton code. */
bool ookSameTransmitter(const OokPacket &a, const OokPacket &b);

/* Short label for lists: "PRN" or "LAX". */
const char *ookDecoderShort(OokDecoder d);

class OokModel {
public:
    static constexpr size_t kMaxPackets = 32;
    static constexpr size_t kMaxDevices = 16;

    /* ook_listen confirmed: a fresh session. */
    void begin();
    /* [STOP] for the OOK lane: keep everything for a last look. */
    void stop() { active_ = false; }
    /* Probe rebooted: nothing here is trustworthy any more. */
    void clear();

    /* Absorb an [OOK] reply (ook_listen or ook_status). Frequency and baud
     * come from every reply; health counters only from a complete set, so a
     * stale number never masquerades as current. Returns true when the
     * health counters were updated. */
    bool absorbReply(const ocp::Item &reply);

    /* Absorb an [EVT] kind=ook or kind=ook_unknown. Returns the stored
     * packet for kind=ook, nullptr otherwise (including malformed). */
    const OokPacket *absorbEvent(const ocp::Item &evt, uint32_t now_ms);

    bool active() const { return active_; }
    uint32_t freqKhz() const { return freq_khz_; }
    uint32_t baud() const { return baud_; }
    uint32_t totalPackets() const { return total_; }
    uint32_t unknownTrains() const { return unknown_total_; }
    const OokUnknown &lastUnknown() const { return last_unknown_; }
    const std::vector<OokPacket> &packets() const { return packets_; }
    const std::vector<OokDevice> &devices() const { return devices_; }
    const OokHealth &health() const { return health_; }

private:
    void upsertDevice(const OokPacket &p);

    bool active_ = false;
    uint32_t freq_khz_ = 0;
    uint32_t baud_ = 0;
    uint32_t total_ = 0;
    uint32_t unknown_total_ = 0;
    OokUnknown last_unknown_;
    OokHealth health_;
    std::vector<OokPacket> packets_;   /* index 0 = most recent */
    std::vector<OokDevice> devices_;   /* index 0 = most recently heard */
};

}  // namespace model
