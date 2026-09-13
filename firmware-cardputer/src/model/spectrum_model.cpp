/*
 * spectrum_model.cpp — see spectrum_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/spectrum_model.h"

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

void SpectrumModel::begin()
{
    active_ = true;
    locked_ = false;
    locked_ch_ = 0;
    readings_.clear();
}

void SpectrumModel::beginLocked(uint8_t ch)
{
    active_ = true;
    locked_ = true;
    locked_ch_ = ch;
    readings_.clear();
    readings_.push_back({ ch, ch > 14, 0 });
}

void SpectrumModel::clear()
{
    active_ = false;
    locked_ = false;
    locked_ch_ = 0;
    readings_.clear();
}

void SpectrumModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_CHAN) return;

    long ch = kvLong(evt, OCP_K_CH, 1, 255, -1);
    long pkts = kvLong(evt, OCP_K_PKTS, 0, 0x7fffffffL, -1);
    if (ch < 0 || pkts < 0) return;   /* malformed: never trust a partial event */

    if (locked_) {
        if (!readings_.empty()) readings_[0].pkts = static_cast<uint32_t>(pkts);
        return;
    }
    for (auto &r : readings_) {
        if (r.ch == ch) {
            r.pkts = static_cast<uint32_t>(pkts);
            return;
        }
    }
    readings_.push_back({ static_cast<uint8_t>(ch), ch > 14, static_cast<uint32_t>(pkts) });
}

}  // namespace model
