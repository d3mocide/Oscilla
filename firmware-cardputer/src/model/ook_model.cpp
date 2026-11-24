/*
 * ook_model.cpp — see ook_model.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/ook_model.h"

#include <cerrno>
#include <cstdlib>

#include "ocp.h"

namespace model {

namespace {

/* Whole-token decimal in [lo, hi]; false when absent or malformed. */
bool kvLong(const ocp::Item &it, const char *key, long lo, long hi, long *out)
{
    const auto *s = it.get(key);
    if (!s || s->empty()) return false;
    char *end = nullptr;
    errno = 0;
    const long v = std::strtol(s->c_str(), &end, 10);
    if (errno || *end || v < lo || v > hi) return false;
    *out = v;
    return true;
}

bool isLowerHex(const std::string &s)
{
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

bool sameDevice(const OokPacket &a, const OokPacket &b)
{
    if (a.decoder != b.decoder) return false;
    if (a.decoder == OokDecoder::LaCrosse) return a.id == b.id && a.ch == b.ch;
    return a.nbits == b.nbits && a.hex == b.hex;   /* a remote is its code */
}

constexpr long kU32Max = 2147483647L;   /* counters beyond this are not plausible */

}  // namespace

const char *ookDecoderShort(OokDecoder d)
{
    return d == OokDecoder::LaCrosse ? "LAX" : "PRN";
}

void OokModel::begin()
{
    active_ = true;
    total_ = 0;
    unknown_total_ = 0;
    last_unknown_ = {};
    health_ = {};
    packets_.clear();
    devices_.clear();
}

void OokModel::clear()
{
    begin();
    active_ = false;
    freq_khz_ = 0;
    baud_ = 0;
}

bool OokModel::absorbReply(const ocp::Item &reply)
{
    if (reply.tag != OCP_MARK_OOK) return false;
    long v = 0;
    if (kvLong(reply, OCP_K_FREQ_KHZ, 300000, 928000, &v)) freq_khz_ = static_cast<uint32_t>(v);
    if (kvLong(reply, OCP_K_BAUD, 1, 1000000, &v)) baud_ = static_cast<uint32_t>(v);
    if (kvLong(reply, OCP_K_RUNNING, 0, 1, &v) && v == 0) active_ = false;

    static const char *const kKeys[] = {OCP_K_TRAINS, OCP_K_PACKETS, OCP_K_UNKNOWN, OCP_K_OVERFLOWS,
                                        OCP_K_DROPPED, OCP_K_ERRORS, OCP_K_REINIT, OCP_K_HW_FAULT,
                                        OCP_K_OCP_DROP};
    long c[sizeof kKeys / sizeof kKeys[0]];
    for (size_t i = 0; i < sizeof kKeys / sizeof kKeys[0]; ++i) {
        if (!kvLong(reply, kKeys[i], 0, kU32Max, &c[i])) return false;
    }
    health_.valid = true;
    health_.trains = static_cast<uint32_t>(c[0]);
    health_.packets = static_cast<uint32_t>(c[1]);
    health_.unknown = static_cast<uint32_t>(c[2]);
    health_.overflows = static_cast<uint32_t>(c[3]);
    health_.dropped = static_cast<uint32_t>(c[4]);
    health_.errors = static_cast<uint32_t>(c[5]);
    health_.reinit = static_cast<uint32_t>(c[6]);
    health_.hw_fault = static_cast<uint32_t>(c[7]);
    health_.ocp_drop = static_cast<uint32_t>(c[8]);
    return true;
}

const OokPacket *OokModel::absorbEvent(const ocp::Item &evt, uint32_t now_ms)
{
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind) return nullptr;
    long v = 0;

    if (*kind == OCP_EVT_KIND_OOK_UNKNOWN) {
        long n = 0, pulses = 0, mn = 0, mx = 0, dur = 0;
        if (!kvLong(evt, OCP_K_COUNT, 1, kU32Max, &n) || !kvLong(evt, OCP_K_PULSES, 0, 65535, &pulses) ||
            !kvLong(evt, OCP_K_MIN_US, 0, 65535, &mn) || !kvLong(evt, OCP_K_MAX_US, 0, 65535, &mx) ||
            !kvLong(evt, OCP_K_DUR_MS, 0, kU32Max, &dur) || mn > mx) {
            return nullptr;
        }
        OokUnknown u;
        u.valid = true;
        u.received_ms = now_ms;
        u.pulses = static_cast<uint32_t>(pulses);
        u.min_us = static_cast<uint32_t>(mn);
        u.max_us = static_cast<uint32_t>(mx);
        u.dur_ms = static_cast<uint32_t>(dur);
        if (evt.get(OCP_K_RSSI)) {
            if (!kvLong(evt, OCP_K_RSSI, -200, 0, &v)) return nullptr;
            u.has_rssi = true;
            u.rssi = static_cast<int>(v);
        }
        last_unknown_ = u;
        unknown_total_ += static_cast<uint32_t>(n);
        return nullptr;
    }
    if (*kind != OCP_EVT_KIND_OOK) return nullptr;

    OokPacket p;
    const auto *dec = evt.get(OCP_K_DECODER);
    if (!dec) return nullptr;
    if (*dec == OCP_OOK_DECODER_PRINCETON) p.decoder = OokDecoder::Princeton;
    else if (*dec == OCP_OOK_DECODER_LACROSSE) p.decoder = OokDecoder::LaCrosse;
    else return nullptr;   /* a decoder this deck doesn't know: skip, don't guess */

    long nbits = 0, frames = 0, te = 0;
    const auto *hex = evt.get(OCP_K_HEX);
    if (!kvLong(evt, OCP_K_NBITS, 1, 40, &nbits) || !kvLong(evt, OCP_K_FRAMES, 1, 255, &frames) ||
        !kvLong(evt, OCP_K_TE_US, 1, 65535, &te) || !hex ||
        hex->size() != static_cast<size_t>((nbits + 7) / 8) * 2 || !isLowerHex(*hex)) {
        return nullptr;
    }
    p.received_ms = now_ms;
    p.nbits = static_cast<uint8_t>(nbits);
    p.hex = *hex;
    p.frames = static_cast<uint16_t>(frames);
    p.te_us = static_cast<uint16_t>(te);
    if (evt.get(OCP_K_RSSI)) {
        if (!kvLong(evt, OCP_K_RSSI, -200, 0, &v)) return nullptr;
        p.has_rssi = true;
        p.rssi = static_cast<int>(v);
    }

    if (p.decoder == OokDecoder::Princeton) {
        if (p.nbits != 24) return nullptr;
    } else {
        long id = 0, ch = 0, bl = 0, test = 0, t = 0, hum = 0;
        if (p.nbits != 40 || !kvLong(evt, OCP_K_ID, 0, 255, &id) || !kvLong(evt, OCP_K_CH, 0, 3, &ch) ||
            !kvLong(evt, OCP_K_BATT_LOW, 0, 1, &bl) || !kvLong(evt, OCP_K_TEST, 0, 1, &test) ||
            !kvLong(evt, OCP_K_TEMP_C10, -500, 3595, &t) || !kvLong(evt, OCP_K_HUM, 0, 255, &hum)) {
            return nullptr;
        }
        p.id = static_cast<uint8_t>(id);
        p.ch = static_cast<uint8_t>(ch);
        p.batt_low = bl != 0;
        p.test = test != 0;
        p.temp_c10 = static_cast<int>(t);
        p.hum = static_cast<uint8_t>(hum);
    }

    total_++;
    packets_.insert(packets_.begin(), p);
    if (packets_.size() > kMaxPackets) packets_.resize(kMaxPackets);
    upsertDevice(p);
    return &packets_.front();
}

void OokModel::upsertDevice(const OokPacket &p)
{
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (!sameDevice(devices_[i].last, p)) continue;
        OokDevice d = devices_[i];
        d.last = p;
        d.count++;
        devices_.erase(devices_.begin() + static_cast<long>(i));
        devices_.insert(devices_.begin(), d);
        return;
    }
    OokDevice d;
    d.last = p;
    d.first_ms = p.received_ms;
    d.count = 1;
    devices_.insert(devices_.begin(), d);
    if (devices_.size() > kMaxDevices) devices_.resize(kMaxDevices);   /* least recently heard goes */
}

}  // namespace model
