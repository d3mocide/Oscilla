/*
 * lora_model.cpp — see lora_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/lora_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"
#include "model/number_parse.h"

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
    double parsed = 0.0;
    if (!parseFiniteDouble(*s, &parsed) || parsed < lo || parsed > hi) return false;
    *out = static_cast<float>(parsed);
    return true;
}

}  // namespace

void LoraModel::configured(uint32_t freq_hz, int sf, int bw_khz, int cr, const char *profile)
{
    has_config_ = true;
    freq_hz_ = freq_hz;
    sf_ = sf;
    bw_khz_ = bw_khz;
    cr_ = cr;
    profile_ = profile && *profile ? profile : "manual";
}

void LoraModel::begin()
{
    active_ = true;
    total_ = 0;
    packets_.clear();
    health_ = {};
}

void LoraModel::clear()
{
    active_ = false;
    has_config_ = false;
    profile_ = "manual";
    total_ = 0;
    packets_.clear();
    health_ = {};
}

bool LoraModel::absorbStatus(const ocp::Item &reply)
{
    if (reply.tag != OCP_MARK_LORA) return false;

    const long rx = kvLong(reply, OCP_K_RX, 0, 2147483647L, -1);
    const long crc = kvLong(reply, OCP_K_CRC_ERR, 0, 2147483647L, -1);
    const long header = kvLong(reply, OCP_K_HEADER_ERR, 0, 2147483647L, -1);
    const long irq_drop = kvLong(reply, OCP_K_IRQ_DROP, 0, 2147483647L, -1);
    const long radio_drop = kvLong(reply, OCP_K_RADIO_DROP, 0, 2147483647L, -1);
    const long ocp_drop = kvLong(reply, OCP_K_OCP_DROP, 0, 2147483647L, -1);
    if (rx < 0 || crc < 0 || header < 0 || irq_drop < 0 || radio_drop < 0 || ocp_drop < 0) return false;

    health_.valid = true;
    health_.rx = static_cast<uint32_t>(rx);
    health_.crc_err = static_cast<uint32_t>(crc);
    health_.header_err = static_cast<uint32_t>(header);
    health_.irq_drop = static_cast<uint32_t>(irq_drop);
    health_.radio_drop = static_cast<uint32_t>(radio_drop);
    health_.ocp_drop = static_cast<uint32_t>(ocp_drop);
    return true;
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
    p.framing = classifyLoraFrameHex(p.hex);

    total_++;
    packets_.insert(packets_.begin(), p);
    if (packets_.size() > kMaxRows) packets_.resize(kMaxRows);
    return &packets_.front();
}

}  // namespace model
