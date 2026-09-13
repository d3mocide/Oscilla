/*
 * contacts_model.h — the deck's view of the promiscuous sniffer (OCP-SPEC
 * §10.4): AP<->client links and probe-request SSIDs. Framework-agnostic;
 * host-tested in test/host/contacts_model_test.cpp.
 *
 * The authoritative tables come only from [CLIENTS]/[PROBES] snapshots,
 * requested periodically while the view is open — matching the OCP-SPEC
 * §10.4 promise that these are lossy, best-effort tables. [EVT] kind=sniff/
 * client/probe update only the live ticker (last sighting, packet count,
 * current channel): a dropped event must never desync the shown list.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ocp/ocp_item.h"

namespace model {

struct ClientRow {
    std::string bssid;
    std::string mac;
    uint8_t ch = 0;
    bool band5 = false;
    int rssi = 0;
    uint32_t pkts = 0;
};

struct ProbeRow {
    std::string mac;
    std::string ssid;      /* raw bytes: re-escape before display */
    int rssi = 0;
    uint32_t pkts = 0;
};

class ContactsModel {
public:
    static constexpr size_t kMaxRows = 256;   /* matches the probe's SNIFF_*_MAX headroom */

    /* start_sniffer was (re)issued: forget the old session. */
    void begin();

    /* Probe rebooted: its tables are gone, so ours are meaningless. */
    void clear();

    /* [STOP] landed: the probe's tables persist, only the live state stops. */
    void stopSniffing() { sniffing_ = false; }

    /* Absorb a [CLIENTS] / [PROBES] snapshot frame wholesale. */
    void absorbClients(const ocp::Item &frame);
    void absorbProbes(const ocp::Item &frame);

    /* Absorb an [EVT] kind=sniff|client|probe: updates the ticker only. */
    void absorbEvent(const ocp::Item &evt);

    bool sniffing() const { return sniffing_; }
    const std::vector<ClientRow> &clients() const { return clients_; }
    const std::vector<ProbeRow> &probes() const { return probes_; }
    uint16_t malformedRows() const { return malformed_; }

    uint32_t totalPkts() const { return total_pkts_; }
    uint8_t currentChannel() const { return current_ch_; }
    const std::string &lastSighting() const { return last_sighting_; }

private:
    std::vector<ClientRow> clients_;
    std::vector<ProbeRow> probes_;
    uint16_t malformed_ = 0;
    bool sniffing_ = false;

    uint32_t total_pkts_ = 0;
    uint8_t current_ch_ = 0;
    std::string last_sighting_;
};

}  // namespace model
