/*
 * legacy_model.cpp — see legacy_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/legacy_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"

namespace model {

namespace {

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

void LegacyModel::begin()
{
    active_ = true;
    total_ = 0;
    packets_.clear();
}

void LegacyModel::clear()
{
    active_ = false;
    has_config_ = false;
    total_ = 0;
    packets_.clear();
}

const LegacyPacket *LegacyModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_LEGACY) return nullptr;

    const auto *hex = evt.get(OCP_K_HEX);
    long len = kvLong(evt, OCP_K_LEN, 0, 64, -1);
    if (!hex || len < 0 || hex->size() != static_cast<size_t>(len) * 2) {
        return nullptr;   /* malformed: never trust a partial event */
    }

    LegacyPacket p;
    p.rssi = static_cast<int>(kvLong(evt, OCP_K_RSSI, -200, 0, 0));
    p.len = static_cast<uint16_t>(len);
    p.hex = *hex;

    total_++;
    packets_.insert(packets_.begin(), p);
    if (packets_.size() > kMaxRows) packets_.resize(kMaxRows);
    return &packets_.front();
}

}  // namespace model
