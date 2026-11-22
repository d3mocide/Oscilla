/*
 * signal_hunt.cpp — see signal_hunt.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/signal_hunt.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>

#include "ocp.h"

namespace model {

namespace {

std::string lowerStr(const std::string &s)
{
    std::string out = s;
    for (char &c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool parseRssi(const std::string &s, int *out)
{
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno || !end || *end != '\0' || v < -127 || v > 0) return false;
    *out = static_cast<int>(v);
    return true;
}

}  // namespace

void SignalHunt::startWifi(const std::string &bssid, const std::string &ssid,
                           uint8_t ch, bool audio_enabled)
{
    clear();
    kind_ = HuntKind::WifiAp;
    mac_ = lowerStr(bssid);
    label_ = ssid;
    channel_ = ch;
    audio_enabled_ = audio_enabled;
}

void SignalHunt::startBt(const std::string &mac, const std::string &name, bool audio_enabled)
{
    clear();
    kind_ = HuntKind::BtDevice;
    mac_ = lowerStr(mac);
    label_ = name;
    channel_ = 0;
    audio_enabled_ = audio_enabled;
}

void SignalHunt::stop()
{
    streaming_ = false;
}

void SignalHunt::clear()
{
    kind_ = HuntKind::None;
    mac_.clear();
    label_.clear();
    channel_ = 0;
    streaming_ = false;
    samples_ = 0;
    current_rssi_ = -120;
    peak_rssi_ = -120;
    min_rssi_ = 0;
    last_seen_ms_ = 0;
    last_tick_ms_ = 0;
    has_ticked_ = false;
    history_.fill(-120);
    history_head_ = 0;
    history_count_ = 0;
}

void SignalHunt::resetPeak()
{
    if (samples_ > 0) {
        peak_rssi_ = current_rssi_;
        min_rssi_ = current_rssi_;
    } else {
        peak_rssi_ = -120;
        min_rssi_ = 0;
    }
}

void SignalHunt::observeSample(int rssi_dbm, uint32_t now_ms)
{
    if (!active()) return;
    const int clamped = std::max(-120, std::min(-10, rssi_dbm));
    current_rssi_ = clamped;
    if (samples_ == 0) {
        peak_rssi_ = clamped;
        min_rssi_ = clamped;
    } else {
        if (clamped > peak_rssi_) peak_rssi_ = clamped;
        if (clamped < min_rssi_) min_rssi_ = clamped;
    }
    ++samples_;
    last_seen_ms_ = now_ms;

    history_[history_head_] = static_cast<int8_t>(clamped);
    history_head_ = (history_head_ + 1u) % kHistoryCap;
    if (history_count_ < kHistoryCap) ++history_count_;
}

bool SignalHunt::absorbEvent(const ocp::Item &evt, uint32_t now_ms)
{
    if (!active() || evt.kind != ocp::ItemKind::Event) return false;
    const auto *kind = evt.get(OCP_K_KIND);
    if (!kind || *kind != OCP_EVT_KIND_HUNT) return false;

    const auto *mac = evt.get(OCP_K_MAC);
    const auto *rssi_s = evt.get(OCP_K_RSSI);
    int rssi = -120;
    if (!mac || !rssi_s || lowerStr(*mac) != mac_ || !parseRssi(*rssi_s, &rssi)) return false;

    observeSample(rssi, now_ms);
    return true;
}

uint32_t SignalHunt::geigerIntervalMs(int rssi_dbm)
{
    /* Map -100 dBm -> 1200 ms down to -35 dBm -> 85 ms monotonically. */
    const int c = std::max(-100, std::min(-35, rssi_dbm));
    const int pct = (c + 100) * 100 / 65;   /* 0..100 */
    return static_cast<uint32_t>(1200 - (pct * 1115) / 100);
}

uint16_t SignalHunt::geigerFreqHz(int rssi_dbm)
{
    /* Map -100 dBm -> 520 Hz up to -35 dBm -> 2200 Hz monotonically. */
    const int c = std::max(-100, std::min(-35, rssi_dbm));
    const int pct = (c + 100) * 100 / 65;
    return static_cast<uint16_t>(520 + (pct * 1680) / 100);
}

const char *SignalHunt::proximityLabel(int rssi_dbm, bool stale)
{
    if (stale) return "STALE";
    if (rssi_dbm >= -48) return "HOT";
    if (rssi_dbm >= -62) return "NEAR";
    if (rssi_dbm >= -78) return "MID";
    return "FAR";
}

GeigerPulse SignalHunt::pollGeiger(uint32_t now_ms)
{
    GeigerPulse p{};
    if (!active() || !audio_enabled_ || isStale(now_ms)) return p;

    const uint32_t interval = geigerIntervalMs(current_rssi_);
    if (!has_ticked_ || (now_ms - last_tick_ms_) >= interval) {
        has_ticked_ = true;
        last_tick_ms_ = now_ms;
        p.fire = true;
        p.freq_hz = geigerFreqHz(current_rssi_);
        p.duration_ms = 18;
    }
    return p;
}

int8_t SignalHunt::historyAt(size_t idx) const
{
    if (idx >= history_count_) return -120;
    const size_t start = (history_head_ + kHistoryCap - history_count_) % kHistoryCap;
    return history_[(start + idx) % kHistoryCap];
}

}  // namespace model
