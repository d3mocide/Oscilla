/* Host tests for model::SignalHunt (WP-12b). */
#include "app/audio_cues.h"
#include "model/signal_hunt.h"
#include "ocp.h"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

ocp::Item huntEvt(const std::string &mac, int rssi, int ch)
{
    ocp::Item it;
    it.kind = ocp::ItemKind::Event;
    it.kv = {{OCP_K_KIND, OCP_EVT_KIND_HUNT},
             {OCP_K_MAC, mac},
             {OCP_K_RSSI, std::to_string(rssi)},
             {OCP_K_CH, std::to_string(ch)}};
    return it;
}

}  // namespace

int main()
{
    model::SignalHunt h;
    assert(!h.active() && !h.pollGeiger(1000).fire);

    // Start muted by default: samples record cleanly, Geiger stays silent.
    h.startWifi("AA:BB:CC:DD:EE:01", "TargetAP", 6, false);
    assert(h.active() && h.kind() == model::HuntKind::WifiAp && !h.audioEnabled());
    assert(h.isStale(1000));

    // Foreign MAC ignored; matching MAC accepted (case-insensitive).
    assert(!h.absorbEvent(huntEvt("aa:bb:cc:dd:ee:99", -40, 6), 1000));
    assert(h.samples() == 0);
    assert(h.absorbEvent(huntEvt("aa:bb:cc:dd:ee:01", -82, 6), 1000));
    assert(h.samples() == 1 && h.currentRssi() == -82 && h.peakRssi() == -82);
    assert(!h.isStale(1500));
    assert(!h.pollGeiger(1500).fire);  // muted

    // Unmute: Geiger fires with monotonic pitch/cadence scaling.
    h.setAudioEnabled(true);
    auto p_far = h.pollGeiger(1500);
    assert(p_far.fire && p_far.duration_ms == 18);
    assert(!h.pollGeiger(1600).fire);  // rate-limited by interval

    // Closer signal (-42 dBm) -> faster cadence + higher pitch + updated peak.
    assert(h.absorbEvent(huntEvt("aa:bb:cc:dd:ee:01", -42, 6), 1700));
    assert(h.peakRssi() == -42 && h.minRssi() == -82);
    assert(model::SignalHunt::geigerIntervalMs(-42) < model::SignalHunt::geigerIntervalMs(-82));
    assert(model::SignalHunt::geigerFreqHz(-42) > model::SignalHunt::geigerFreqHz(-82));
    assert(std::strcmp(model::SignalHunt::proximityLabel(-42, false), "HOT") == 0);
    assert(std::strcmp(model::SignalHunt::proximityLabel(-42, true), "STALE") == 0);

    // Stale target (> 5000 ms since last_seen) silences Geiger automatically.
    assert(h.isStale(1700 + model::SignalHunt::kStaleMs + 1));
    assert(!h.pollGeiger(1700 + model::SignalHunt::kStaleMs + 1).fire);

    // Reset peak + ring wrap past 64 samples preserves chronological oldest->newest order.
    h.resetPeak();
    assert(h.peakRssi() == -42);
    for (int i = 0; i < 80; ++i) {
        h.observeSample(-100 + (i % 50), static_cast<uint32_t>(10000 + i * 100));
    }
    assert(h.historyCount() == model::SignalHunt::kHistoryCap);
    assert(h.historyAt(0) == -100 + (16 % 50));
    assert(h.historyAt(model::SignalHunt::kHistoryCap - 1) == -100 + (79 % 50));

    // Clock wraparound safety on Geiger cadence.
    h.observeSample(-40, 0xFFFFFF80u);
    auto p_wrap1 = h.pollGeiger(0xFFFFFF80u);
    assert(p_wrap1.fire);
    assert(!h.pollGeiger(0xFFFFFFA0u).fire);
    auto p_wrap2 = h.pollGeiger(0x00000100u);
    assert(p_wrap2.fire);

    app::AudioCues cues;
    cues.updateHunt(true, false);
    assert(!cues.busy());
    cues.updateHunt(true, true);
    const auto acquire_first = cues.poll(0xffffffc0u);
    assert(acquire_first.fire);
    assert(!cues.poll(20).fire);
    cues.updateHunt(true, true);
    const auto acquire_second = cues.poll(30);
    assert(acquire_second.fire && acquire_second.freq_hz > acquire_first.freq_hz);
    assert(cues.busy() && !cues.poll(125).fire);
    assert(!cues.poll(150).fire && !cues.busy());
    cues.updateHunt(true, false);
    const auto lost_first = cues.poll(200);
    const auto lost_second = cues.poll(300);
    assert(lost_first.fire && lost_second.fire && lost_second.freq_hz < lost_first.freq_hz);
    cues.updateHunt(false, false);
    assert(!cues.busy());

    cues.waypoint();
    assert(cues.poll(1100).fire);
    cues.clear();
    assert(!cues.poll(1200).fire && !cues.busy());

    std::cout << "signal hunt: target filter, peak/sparkline ring, monotonic Geiger cadence stale mute and cue sequencing passed\n";
    return 0;
}
