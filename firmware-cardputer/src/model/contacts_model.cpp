/*
 * contacts_model.cpp — see contacts_model.h. The probe's rows are validated,
 * never trusted: a malformed row is counted and skipped (mirrors scan_model.cpp).
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/contacts_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"
#include "ocp/ocp_csv.h"

namespace model {

namespace {

bool toLong(const std::string &s, long lo, long hi, long &out)
{
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno || *end || v < lo || v > hi) return false;
    out = v;
    return true;
}

long kvLong(const ocp::Item &it, const char *key, long lo, long hi, long dflt)
{
    long v = dflt;
    if (const auto *s = it.get(key)) toLong(*s, lo, hi, v);
    return v;
}

bool looksLikeMac(const std::string &s) { return s.size() == 17; }

bool parseClientRow(const std::string &raw, ClientRow &row)
{
    std::vector<std::string> f;
    if (!ocp::splitCsvRow(raw, f) || f.size() != OCP_CLIENTS_CSV_FIELDS) return false;

    long ch, rssi;
    if (!looksLikeMac(f[0]) || !looksLikeMac(f[1])) return false;
    if (!toLong(f[2], 1, 196, ch)) return false;
    if (f[3] != OCP_BAND_LABEL_24 && f[3] != OCP_BAND_LABEL_5) return false;
    if (!toLong(f[4], -128, 127, rssi)) return false;

    row.bssid = f[0];
    row.mac = f[1];
    row.ch = static_cast<uint8_t>(ch);
    row.band5 = f[3] == OCP_BAND_LABEL_5;
    row.rssi = static_cast<int>(rssi);
    row.pkts = static_cast<uint32_t>(std::strtoul(f[5].c_str(), nullptr, 10));
    return true;
}

bool parseProbeRow(const std::string &raw, ProbeRow &row)
{
    std::vector<std::string> f;
    if (!ocp::splitCsvRow(raw, f) || f.size() != OCP_PROBES_CSV_FIELDS) return false;

    long rssi;
    if (!looksLikeMac(f[0])) return false;
    if (!toLong(f[2], -128, 127, rssi)) return false;

    row.mac = f[0];
    row.ssid = f[1];
    row.rssi = static_cast<int>(rssi);
    row.pkts = static_cast<uint32_t>(std::strtoul(f[3].c_str(), nullptr, 10));
    return true;
}

}  // namespace

void ContactsModel::begin()
{
    clients_.clear();
    clients_.shrink_to_fit();
    probes_.clear();
    probes_.shrink_to_fit();
    malformed_ = 0;
    sniffing_ = true;
    total_pkts_ = 0;
    current_ch_ = 0;
    last_sighting_.clear();
}

void ContactsModel::clear()
{
    begin();
    sniffing_ = false;
}

void ContactsModel::absorbClients(const ocp::Item &frame)
{
    if (frame.tag != OCP_MARK_CLIENTS) return;

    std::vector<ClientRow> rows;
    uint16_t bad = 0;
    for (const auto &raw : frame.rows) {
        if (rows.size() >= kMaxRows) break;
        ClientRow row;
        if (parseClientRow(raw, row)) rows.push_back(std::move(row));
        else bad++;
    }
    clients_ = std::move(rows);
    malformed_ = bad;   /* a fresh snapshot: last frame's count doesn't carry over */
}

void ContactsModel::absorbProbes(const ocp::Item &frame)
{
    if (frame.tag != OCP_MARK_PROBES) return;

    std::vector<ProbeRow> rows;
    uint16_t bad = 0;
    for (const auto &raw : frame.rows) {
        if (rows.size() >= kMaxRows) break;
        ProbeRow row;
        if (parseProbeRow(raw, row)) rows.push_back(std::move(row));
        else bad++;
    }
    probes_ = std::move(rows);
    malformed_ = bad;
}

void ContactsModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind) return;

    if (*kind == OCP_EVT_KIND_SNIFF) {
        total_pkts_ = static_cast<uint32_t>(kvLong(evt, OCP_K_PKTS, 0, 0x7fffffffL, total_pkts_));
        current_ch_ = static_cast<uint8_t>(kvLong(evt, OCP_K_CH, 0, 196, current_ch_));
    } else if (*kind == OCP_EVT_KIND_CLIENT) {
        const auto *mac = evt.get(OCP_K_MAC);
        const auto *bssid = evt.get(OCP_K_BSSID);
        last_sighting_ = "client " + (mac ? *mac : std::string("?")) +
                         " -> " + (bssid ? *bssid : std::string("?"));
    } else if (*kind == OCP_EVT_KIND_PROBE) {
        const auto *mac = evt.get(OCP_K_MAC);
        const auto *ssid = evt.get(OCP_K_SSID);
        std::string name = (ssid && !ssid->empty()) ? *ssid : std::string("<wildcard>");
        last_sighting_ = "probe " + (mac ? *mac : std::string("?")) + " -> " + name;
    }
}

}  // namespace model
