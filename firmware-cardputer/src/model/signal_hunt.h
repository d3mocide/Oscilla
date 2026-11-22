/*
 * signal_hunt.h — single-target passive RSSI Signal Hunt model & Geiger
 * cadence calculator (WP-12b). Framework-agnostic, allocation-free ring;
 * host-tested in test/host/signal_hunt_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ocp/ocp_item.h"

namespace model {

enum class HuntKind : uint8_t {
    None = 0,
    WifiAp = 1,
    BtDevice = 2,
};

struct GeigerPulse {
    bool fire = false;
    uint16_t freq_hz = 0;
    uint16_t duration_ms = 0;
};

class SignalHunt {
public:
    static constexpr size_t kHistoryCap = 64;
    static constexpr uint32_t kStaleMs = 5000;

    void startWifi(const std::string &bssid, const std::string &ssid, uint8_t ch, bool audio_enabled);
    void startBt(const std::string &mac, const std::string &name, bool audio_enabled);
    void setStreaming(bool on) { streaming_ = on; }
    void stop();
    void clear();
    void resetPeak();
    void setAudioEnabled(bool on) { audio_enabled_ = on; }

    /* Absorb `[EVT] kind=hunt mac=... rssi=... ch=...` when matching target. */
    bool absorbEvent(const ocp::Item &evt, uint32_t now_ms);
    /* Direct sample injection (also used by host tests). */
    void observeSample(int rssi_dbm, uint32_t now_ms);

    /* Pure Geiger cadence calculation; fires only when active, unmuted, fresh. */
    GeigerPulse pollGeiger(uint32_t now_ms);

    static uint32_t geigerIntervalMs(int rssi_dbm);
    static uint16_t geigerFreqHz(int rssi_dbm);
    static const char *proximityLabel(int rssi_dbm, bool stale);

    bool active() const { return kind_ != HuntKind::None; }
    bool streaming() const { return streaming_; }
    HuntKind kind() const { return kind_; }
    const std::string &mac() const { return mac_; }
    const std::string &label() const { return label_; }
    uint8_t channel() const { return channel_; }
    bool audioEnabled() const { return audio_enabled_; }

    uint32_t samples() const { return samples_; }
    int currentRssi() const { return current_rssi_; }
    int peakRssi() const { return peak_rssi_; }
    int minRssi() const { return min_rssi_; }
    uint32_t lastSeenMs() const { return last_seen_ms_; }
    bool isStale(uint32_t now_ms) const { return samples_ == 0 || (now_ms - last_seen_ms_) > kStaleMs; }

    /* Chronological oldest -> newest (0 .. historyCount()-1). */
    size_t historyCount() const { return history_count_; }
    int8_t historyAt(size_t idx) const;

private:
    HuntKind kind_ = HuntKind::None;
    std::string mac_;
    std::string label_;
    uint8_t channel_ = 0;
    bool streaming_ = false;
    bool audio_enabled_ = false;

    uint32_t samples_ = 0;
    int current_rssi_ = -120;
    int peak_rssi_ = -120;
    int min_rssi_ = 0;
    uint32_t last_seen_ms_ = 0;
    uint32_t last_tick_ms_ = 0;
    bool has_ticked_ = false;

    std::array<int8_t, kHistoryCap> history_{};
    size_t history_head_ = 0;
    size_t history_count_ = 0;
};

}  // namespace model
