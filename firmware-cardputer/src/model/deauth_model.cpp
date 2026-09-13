/*
 * deauth_model.cpp — see deauth_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/deauth_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"

namespace model {

namespace {

bool looksLikeMac(const std::string &s) { return s.size() == 17; }

long kvLong(const ocp::Item &it, const char *key, long lo, long hi, long dflt)
{
    long v = dflt;
    if (const auto *s = it.get(key)) {
        char *end = nullptr;
        errno = 0;
        long parsed = std::strtol(s->c_str(), &end, 10);
        if (!errno && !s->empty() && !*end && parsed >= lo && parsed <= hi) v = parsed;
    }
    return v;
}

}  // namespace

void DeauthModel::begin()
{
    active_ = true;
    total_ = 0;
    events_.clear();
}

void DeauthModel::clear()
{
    active_ = false;
    total_ = 0;
    events_.clear();
}

void DeauthModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_DEAUTH) return;

    const auto *bssid = evt.get(OCP_K_BSSID);
    const auto *mac = evt.get(OCP_K_MAC);
    if (!bssid || !mac || !looksLikeMac(*bssid) || !looksLikeMac(*mac)) return;   /* malformed: drop it */

    DeauthEvent e;
    e.bssid = *bssid;
    e.mac = *mac;
    e.reason = kvLong(evt, OCP_K_REASON, 0, 65535, 0);
    e.disassoc = kvLong(evt, OCP_K_DISASSOC, 0, 1, 0) == 1;
    e.rssi = static_cast<int>(kvLong(evt, OCP_K_RSSI, -128, 127, 0));
    e.ch = static_cast<uint8_t>(kvLong(evt, OCP_K_CH, 0, 255, 0));

    total_ = static_cast<uint32_t>(kvLong(evt, OCP_K_COUNT, 0, 0x7fffffffL, static_cast<long>(total_) + 1));
    events_.insert(events_.begin(), e);
    if (events_.size() > kMaxRows) events_.resize(kMaxRows);
}

}  // namespace model
