/* Counts/states only; each formatted field has a short stack lifetime. */
#include "app/deck_app.h"
#include "debug/diagnostic_line.h"
#include "model/table_bytes.h"
#include "storage/lora_logger.h"
#include "storage/wardrive_logger.h"
namespace app {
std::string DeckApp::debugSnapshot(uint32_t now_ms) const {
    debug::DiagnosticLine line("dump");
    line.field("link", std::string(ocp::linkStateName(client_.state())));
    const auto &stats=client_.stats();
    line.field("link_errors",stats.errors);
    line.field("link_timeouts",stats.timeouts);
    line.field("link_noise",stats.noise);
    line.field("link_stray",stats.stray);
    line.field("link_resets",stats.resets);
    line.field("probe_owner",probe_owner_);
    line.field("probe_lora",probe_lora_);
    line.field("probe_ook",probe_ook_);
    line.field("ook_active",ook_.active()?1u:0u);
    line.field("ook_devices",static_cast<uint32_t>(ook_.devices().size()));
    line.field("ook_packets",ook_.totalPackets());
    line.field("ook_unknown",ook_.unknownTrains());
    line.field("probe_uptime_ms",probe_uptime_ms_);
    line.field("probe_rst",client_.probe().rst);
    line.field("wifi_region",wifi_region_);
    line.field("probe_region",probe_region_);
    line.field("wifi_band",wifi_band_);
    line.field("probe_band",probe_band_);
    line.field("idle_timeout_s",idle_sleep_.timeoutS());
    line.field("display_sleeping",idle_sleep_.sleeping());
    line.field("battery_pct",cached_battery_pct_);
    line.field("battery_chg",cached_charging_);
    line.field("battery_burn_x10",battery_trend_.burnTenthPctPerHr());
    line.field("battery_est_min",battery_trend_.estMinutes());
    line.field("bt_live",bt_.continuousActive());
    line.field("bt_scanning",bt_.scanning());
    line.field("bt_tracker",bt_.airtagActive());
    line.field("deauth_active",deauth_.active());
    line.field("anti_active",anti_.active());
    line.field("sessions", session_review_.catalog.entries.size());
    line.field("session_stage", int(session_review_.stage));
    line.field("session_busy", session_review_.busy);
    line.field("session_kind", int(session_review_.selected.kind));
    line.field("session_record", session_review_.record);
    line.field("session_parts", session_review_.selected.parts);
    line.field("session_read", session_review_.read_bytes);
    line.field("session_snapshot", session_review_.snapshot_bytes);
    line.field("session_valid", session_review_.summary.valid);
    line.field("session_bad", session_review_.summary.bad);
    line.field("session_page", session_review_.page_index);
    line.field("session_rows", session_review_.rows.size());
    line.field("session_error", !session_review_.error.empty());
    line.field("scan_rows", scan_.rows().size());
    line.field("clients", contacts_.clients().size());
    line.field("probes", contacts_.probes().size());
    line.field("wifi_live", scan_.continuousActive());
    line.field("wifi_scanning", scan_.scanning());
    line.field("wifi_capture", wifi_inspection_.inspect().valid);
    line.field("wifi_beacons", wifi_inspection_.inspect().beacons);
    line.field("wifi_detail", wifi_inspection_.active());
    line.field("wifi_present", wifi_inspection_.present());
    line.field("bt_detail", bt_inspection_.active());
    line.field("bt_present", bt_inspection_.present());
    line.field("bt_samples", bt_inspection_.history().size());
    line.field("contact_detail", contact_inspection_.active());
    line.field("contact_present", contact_inspection_.present());
    line.field("sniffer_active", contacts_.sniffing());
    line.field("sniff_pkts", contacts_.totalPkts());
    line.field("contact_sample_ms", contact_inspection_.sampleMs());
    line.field("client_snapshot_ms", contacts_.clientSnapshotMs());
    line.field("probe_snapshot_ms", contacts_.probeSnapshotMs());
    line.field("spectrum", spectrum_.readings().size());
    line.field("spectrum_active", spectrum_.active());
    line.field("scope_samples", spectrum_.readings().empty() ? 0 : spectrum_.readings()[0].history_size);
    line.field("system_info", showingInfo());
    line.field("probe_status_seq", probe_status_sequence_);
    line.field("zig_pans", zig_.pans().size());
    line.field("zig_nodes", zig_.nodes().size());
    line.field("zig_active", zig_.active());
    line.field("lora_pkts", lora_.packets().size());
    line.field("lora_cfg", (lora_.hasConfig() ? lora_.profile() + "@" + std::to_string(lora_.freqHz()) +
                                                    "/sync" + std::to_string(lora_.syncWord())
                                              : std::string("none")));
    line.field("lora_active", lora_.active());
    line.field("lora_sweep_active", lora_sweep_.active());
    line.field("lora_sweeps", lora_sweep_.sweeps());
    line.field("lora_sweep_peak_dbm", lora_sweep_.hasData() ? static_cast<int>(lora_sweep_.maxPeakDbm()) : 0);
    line.field("lora_profiles", lora_profiles_.size());
    line.field("lora_profile_sd", lora_profiles_.sdCount());
    line.field("lora_selected", lora_profiles_.at(lora_profile_index_).token);
    line.field("lora_rx", lora_.health().rx);
    line.field("lora_hw_fault", lora_.health().hw_fault);
    line.field("deauth_evt", deauth_.events().size());
    line.field("bt_devices", bt_.devices().size());
    line.field("bt_trackers", bt_.deviceTrackerCount());
    line.field("airtag_sightings", bt_.trackerCount());
    line.field("anti_alerts", anti_.alertCount());
    line.field("anti_starting", anti_start_pending_);
    line.field("anti_trackers", anti_.trackers().size());
    line.field("bt_hits", bt_.trackerHits().size());
    {
        using model::tableBytes; using model::stringsHeap;
        const auto &sc = scan_.rows();
        line.field("tbl_scan", tableBytes(sc, sc.size(), [](const model::ApRow &r) { return stringsHeap(r.ssid, r.bssid, r.auth); }));
        const auto &bd = bt_.devices();
        const auto &bh = bt_.trackerHits();
        line.field("tbl_bt", tableBytes(bd, bd.capacity(), [](const model::BtDevice &d) { return stringsHeap(d.mac, d.name, d.mfr, d.tracker_kind); }) +
                             tableBytes(bh, bh.capacity(), [](const model::BtTrackerHit &h) { return stringsHeap(h.mac, h.tracker_kind); }));
        const auto &cc = contacts_.clients();
        const auto &cp = contacts_.probes();
        line.field("tbl_contacts", tableBytes(cc, cc.capacity(), [](const model::ClientRow &r) { return stringsHeap(r.bssid, r.mac); }) +
                                   tableBytes(cp, cp.capacity(), [](const model::ProbeRow &r) { return stringsHeap(r.mac, r.ssid); }));
        line.field("tbl_baseline", baseline_.apHeapBytes());
        const auto &zp = zig_.pans();
        const auto &zn = zig_.nodes();
        line.field("tbl_zig", tableBytes(zp, zp.capacity(), [](const model::ZigPan &p) { return stringsHeap(p.pan, p.proto, p.channels); }) +
                              tableBytes(zn, zn.capacity(), [](const model::ZigNode &n) { return stringsHeap(n.pan, n.short_addr, n.ext, n.role); }));
        const auto &at = anti_.trackers();
        line.field("tbl_anti", tableBytes(at, at.capacity(), [](const model::AntiTracker &t) { return stringsHeap(t.mac, t.tracker_kind); }));
    }
    line.field("scan_evicted", scan_.evicted());
    line.field("bt_evicted", bt_.evicted());
    line.field("probe_heap", probe_heap_);
    line.field("probe_heap_total", probe_heap_total_);
    line.field("probe_heap_min", probe_heap_min_);
    line.field("probe_heap_largest", probe_heap_largest_);
    line.field("probe_psram_total", probe_psram_total_);
    line.field("probe_psram_free", probe_psram_free_);
    line.field("probe_psram_largest", probe_psram_largest_);
    line.field("gnss", std::string(model::gnssStateName(gnss_.state(now_ms))));
    line.field("wardrive_pending", wardriveRowsPending() ? scan_.rows().size() - wardrive_logged_upto_ : 0);
    line.field("wardrive_closing", wardrive_closing_);
    line.field("wardrive_open", storage::wardriveLogStats().open);
    line.field("wardrive_stream", wardrive_stream_active_);
    line.field("wardrive_paused", wardrivePaused());
    line.field("wardrive_block", wardrivePaused() ? wardriveResumeBlocker() : "");
    line.field("wardrive_aps", storage::wardriveLogStats().aps);
    line.field("wardrive_aps_nofix", storage::wardriveLogStats().aps_no_fix);
    line.field("wardrive_bt", storage::wardriveLogStats().bt);
    line.field("wardrive_bt_nofix", storage::wardriveLogStats().bt_no_fix);
    line.field("wardrive_waypoints", storage::wardriveLogStats().waypoints);
    line.field("lora_geotagged", storage::loraLogStats().geotagged_rows);
    line.field("lora_waypoints", storage::loraLogStats().waypoints);
    line.field("session_meta", session_review_.summary.lora_sidecar.has_meta);
    line.field("session_health", session_review_.summary.lora_sidecar.has_health);
    line.field("session_geotagged", session_review_.summary.geotagged);
    line.field("baseline_aps", baseline_.apCount());
    line.field("baseline_pans", baseline_.panCount());
    line.field("baseline_alerts", baseline_.anomalies().size());
    line.field("baseline_high", baseline_.highSeverityCount());
    line.field("baseline_down", baseline_.countOf(model::AnomalyKind::AuthDowngrade));
    line.field("baseline_twin", baseline_.countOf(model::AnomalyKind::EvilTwin));
    line.field("baseline_mfp", baseline_.countOf(model::AnomalyKind::MfpDropped));
    line.field("audio_enabled", audio_enabled_);
    line.field("hunt_active", hunt_.active());
    line.field("hunt_samples", hunt_.samples());
    line.field("hunt_rssi", hunt_.samples() ? hunt_.currentRssi() : 0);
    return line.take();
}
} // namespace app
