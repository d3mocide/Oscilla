/*
 * lora_model.cpp — see lora_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/lora_model.h"

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

/* SnrPkt/4 is always X.Y (one decimal, per lora_recon.c's "%.1f") but stay
 * lenient about what strtof accepts rather than hand-roll a parser. */
bool kvFloat(const ocp::Item &it, const char *key, float lo, float hi, float *out)
{
    const auto *s = it.get(key);
    if (!s || s->empty()) return false;
    char *end = nullptr;
    errno = 0;
    float parsed = std::strtof(s->c_str(), &end);
    if (errno || *end || parsed < lo || parsed > hi) return false;
    *out = parsed;
    return true;
}

}  // namespace

void LoraModel::configured(uint32_t freq_hz, int sf, int bw_khz, int cr)
{
    has_config_ = true;
    freq_hz_ = freq_hz;
    sf_ = sf;
    bw_khz_ = bw_khz;
    cr_ = cr;
}

void LoraModel::begin()
{
    active_ = true;
    total_ = 0;
    packets_.clear();
}

void LoraModel::clear()
{
    active_ = false;
    has_config_ = false;
    total_ = 0;
    packets_.clear();
}

const LoraPacket *LoraModel::absorbEvent(const ocp::Item &evt)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_LORA) return nullptr;

    const auto *hex = evt.get(OCP_K_HEX);
    long len = kvLong(evt, OCP_K_LEN, 0, 255, -1);
    float snr = 0.0f;
    if (!hex || len < 0 || hex->size() != static_cast<size_t>(len) * 2 ||
        !kvFloat(evt, OCP_K_SNR, -128.0f, 127.0f, &snr)) {
        return nullptr;   /* malformed: never trust a partial event */
    }

    LoraPacket p;
    p.rssi = static_cast<int>(kvLong(evt, OCP_K_RSSI, -200, 0, 0));
    p.snr = snr;
    p.len = static_cast<uint16_t>(len);
    p.hex = *hex;

    total_++;
    packets_.insert(packets_.begin(), p);
    if (packets_.size() > kMaxRows) packets_.resize(kMaxRows);
    return &packets_.front();
}

}  // namespace model
