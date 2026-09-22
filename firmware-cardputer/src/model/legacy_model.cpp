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

bool LegacyModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_LEGACY) return false;

    last_rssi_ = static_cast<int>(kvLong(evt, OCP_K_RSSI, -200, 0, last_rssi_));
    total_++;
    return true;
}

}  // namespace model
