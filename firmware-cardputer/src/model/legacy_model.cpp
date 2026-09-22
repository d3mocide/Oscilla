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
    malformed_ = 0;
    fifo_overflows_ = 0;
    queue_drops_ = 0;
    chunks_.clear();
}

void LegacyModel::clear()
{
    active_ = false;
    has_config_ = false;
    total_ = 0;
    malformed_ = 0;
    fifo_overflows_ = 0;
    queue_drops_ = 0;
    chunks_.clear();
}

const LegacyChunk *LegacyModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_LEGACY) return nullptr;   /* not ours, not malformed */

    const auto *hex = evt.get(OCP_K_HEX);
    long len = kvLong(evt, OCP_K_LEN, 0, 64, -1);
    if (!hex || len < 0 || hex->size() != static_cast<size_t>(len) * 2) {
        malformed_++;   /* ours, but never trust a partial event */
        return nullptr;
    }

    LegacyChunk c;
    c.rssi = static_cast<int>(kvLong(evt, OCP_K_RSSI, -200, 0, 0));
    c.len = static_cast<uint16_t>(len);
    c.hex = *hex;

    total_++;
    chunks_.insert(chunks_.begin(), c);
    if (chunks_.size() > kMaxRows) chunks_.resize(kMaxRows);
    return &chunks_.front();
}

void LegacyModel::absorbStatus(const ocp::Item &it)
{
    fifo_overflows_ = static_cast<uint32_t>(kvLong(it, OCP_K_OVERFLOW, 0, 0x7FFFFFFF, fifo_overflows_));
    queue_drops_ = static_cast<uint32_t>(kvLong(it, OCP_K_QDROPS, 0, 0x7FFFFFFF, queue_drops_));
}

}  // namespace model
