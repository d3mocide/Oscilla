/*
 * deck_app.cpp — see deck_app.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/deck_app.h"

#include <cstdlib>

#include "ocp.h"
#include "ui/bt_view.h"
#include "ui/canvas.h"
#include "ui/contacts_view.h"
#include "ui/deauth_view.h"
#include "storage/lora_logger.h"
#include "storage/settings.h"
#include "storage/wardrive_logger.h"
#include "model/number_parse.h"
#include "ui/gnss_view.h"
#include "ui/info_view.h"
#include "ui/link_view.h"
#include "ui/mesh_view.h"
#include "ui/spectrum_view.h"
#include "ui/subghz_view.h"
#include "ui/sweep_view.h"
#include "ui/trace_view.h"

namespace app {

namespace {
constexpr uint32_t kRetryMs = 2000;       /* reconnect while Disconnected */
constexpr uint32_t kKeepaliveMs = 3000;   /* ping when idle, to notice a lost probe */
constexpr uint32_t kRedrawMs = 200;
constexpr uint32_t kBusyRedrawMs = 500;   /* elapsed counter while scanning */
constexpr uint32_t kContactsPollMs = 1500;   /* [CLIENTS]/[PROBES] are the authority, events are just a ticker */
constexpr uint32_t kMeshPollMs = 1500;       /* [ZIG] table is authoritative; events are only first-sighting hints */
constexpr uint32_t kInfoPollMs = 2000;       /* how often to refresh the probe's heap/uptime */
constexpr uint32_t kPendingRetryWindowMs = 4000;   /* generous vs. any single command's own reply timeout */
/* Drive-track vertex cadence. A 1 Hz fix logged raw is 3600 points an hour
 * for a line that only needs enough shape to draw. */
constexpr uint32_t kTrackPointMs = 5000;
/* GNSS diagnostic cadence (2026-09-16, see WORKLOG): tight enough to watch
 * live over USB serial during a bench test, not so tight it floods it. */
constexpr uint32_t kGnssDiagMs = 5000;

/* The home cards: cycled with `,` (left/prev) and `/` (right/next), the
 * physical arrow-key cluster on the Cardputer's keyboard. Trace remains a
 * drill-down from Sweep (DESIGN §7.3). */
constexpr Screen kCards[] = { Screen::Link,   Screen::Sweep,   Screen::Contacts, Screen::Info,
                              Screen::Spectrum, Screen::Mesh, Screen::SubGhz, Screen::Deauth,
                              Screen::Drive, Screen::Beacons };
constexpr int kNumCards = sizeof(kCards) / sizeof(kCards[0]);

bool isHomeCard(Screen s)
{
    for (Screen c : kCards) if (c == s) return true;
    return false;
}

Screen cycleCard(Screen s, int dir)
{
    int i = 0;
    for (; i < kNumCards; i++) if (kCards[i] == s) break;
    i = (i + dir + kNumCards) % kNumCards;
    return kCards[i];
}

/* Debug console only (`card <name>`) — the keyboard never needs this, it
 * reaches Sweep/Trace by drilling down instead (DESIGN §7.3). */
bool screenFromName(const std::string &name, Screen *out)
{
    if (name == "link") *out = Screen::Link;
    else if (name == "sweep") *out = Screen::Sweep;
    else if (name == "trace") *out = Screen::Trace;
    else if (name == "contacts") *out = Screen::Contacts;
    else if (name == "info") *out = Screen::Info;
    else if (name == "spectrum") *out = Screen::Spectrum;
    else if (name == "mesh") *out = Screen::Mesh;
    else if (name == "subghz" || name == "lora") *out = Screen::SubGhz;
    else if (name == "deauth") *out = Screen::Deauth;
    else if (name == "drive") *out = Screen::Drive;
    else if (name == "beacons") *out = Screen::Beacons;
    else return false;
    return true;
}
}  // namespace

DeckApp::DeckApp(ocp::Client::Write write) : client_(std::move(write))
{
    client_.onState([this](ocp::LinkState s) {
        log(std::string("state=") + ocp::linkStateName(s));
        /* A reply timeout skips onReply() entirely (Client::tick() clears
         * pending state internally without calling it), so a lora_listen
         * that never gets answered would leave lora_listen_pending_ stuck
         * true otherwise. Any drop out of Ready means whatever was pending
         * is moot. */
        if (s != ocp::LinkState::Ready) {
            lora_listen_pending_ = false;
            wifi_continuous_pending_ = false;
            mesh_start_pending_ = false;
            anti_start_pending_ = false;
            anti_.stop();
            /* Same reasoning: a queued retry is moot once the link isn't
             * Ready, and must never fire later against something unrelated
             * after a reconnect. */
            pending_retry_ = nullptr;
        }
        dirty_ = true;
    });
    client_.onReset([this] {
        /* The probe's stored results died with it; so did our indices. */
        const auto &st = client_.stats();
        log("probe-reset resets=" + std::to_string(st.resets) + " noise=" + std::to_string(st.noise) +
            " stray=" + std::to_string(st.stray));
        scan_.clear();
        contacts_.clear();
        spectrum_.clear();
        lora_.clear();
        storage::loraLogEnd();
        deauth_.clear();
        anti_.clear();
        mesh_.clear();
        next_page_ = 0;
        if (screen_ == Screen::Trace) screen_ = Screen::Sweep;
        notice("probe rebooted - resynced");
    });
    client_.onReply([this](const ocp::Item &it) { onReply(it); });
    client_.onEvent([this](const ocp::Item &it) { onEvent(it); });
}

void DeckApp::begin(uint32_t now_ms)
{
    client_.connect(now_ms);
}

void DeckApp::notice(const std::string &text)
{
    notice_ = text;
    dirty_ = true;
}

void DeckApp::retrySoon(std::function<void(uint32_t)> action, uint32_t now_ms)
{
    pending_retry_ = std::move(action);
    pending_retry_started_ms_ = now_ms;
    notice("busy, retrying...");
    log("retry queued (pending)");
}

void DeckApp::onReply(const ocp::Item &it)
{
    dirty_ = true;
    if (it.kind == ocp::ItemKind::Pong) { last_reply_ = "pong"; return; }

    if (it.kind == ocp::ItemKind::Error) {
        lora_listen_pending_ = false;   /* rejected: no session, no file (see deck_app.h) */
        if (wifi_continuous_pending_) {
            wifi_continuous_pending_ = false;
            scan_.stop();
        }
        if (mesh_start_pending_) {
            mesh_start_pending_ = false;
            mesh_.stop();
        }
        if (anti_start_pending_) {
            anti_start_pending_ = false;
            anti_.stop();
        }
        const auto *code = it.get(OCP_K_CODE);
        const auto *msg = it.get(OCP_K_MSG);
        /* A same-PHY-lane conflict (radio_arbiter's "radio in use by ...")
         * lands here too, same as any other error: no automatic
         * stop-and-switch. An auto-handoff tried this on 2026-09-14 and it
         * silently killed a concurrently running LoRa session, because the
         * only tool to free the arbiter was a `stop` that took down every
         * lane. That's fixed now (D-16, `stop phy` scopes it, hardware-
         * verified 2026-09-15) and every stop() call site in this file
         * already uses it - re-adding the auto-handoff on top is unblocked,
         * just not done. */
        notice(std::string("error ") + (code ? *code : "?") + ": " + (msg ? *msg : ""));
        log(std::string("err code=") + (code ? *code : "?"));
        return;
    }

    last_reply_ = it.tag;
    if (it.tag == OCP_MARK_CFG && wifi_continuous_pending_) {
        wifi_continuous_pending_ = false;
    }
    if (it.tag == OCP_MARK_CFG && anti_start_pending_) {
        anti_start_pending_ = false;
        anti_.begin();
        anti_.observePosition(gnss_.fix().lat_deg, gnss_.fix().lon_deg,
                              gnss_.state(now_) == model::GnssState::Fixed,
                              gnss_.fixAgeMs(now_), now_);
        notice("");
    }
    if (it.tag == OCP_MARK_STATUS) {
        const auto *heap = it.get(OCP_K_HEAP);
        const auto *uptime = it.get(OCP_K_UPTIME_MS);
        if (heap && uptime) {
            uint64_t heap_value = 0, uptime_value = 0;
            if (model::parseUnsigned(*heap, &heap_value) && heap_value <= UINT32_MAX &&
                model::parseUnsigned(*uptime, &uptime_value)) {
                probe_heap_ = static_cast<uint32_t>(heap_value);
                probe_uptime_ms_ = uptime_value;
                const auto readOptional = [&](const char *key, uint32_t *out) {
                    const auto *value = it.get(key);
                    uint64_t parsed = 0;
                    if (value && model::parseUnsigned(*value, &parsed) && parsed <= UINT32_MAX) {
                        *out = static_cast<uint32_t>(parsed);
                    }
                };
                readOptional(OCP_K_HEAP_MIN, &probe_heap_min_);
                readOptional(OCP_K_HEAP_LARGEST, &probe_heap_largest_);
                readOptional(OCP_K_PSRAM_TOTAL, &probe_psram_total_);
                readOptional(OCP_K_PSRAM_FREE, &probe_psram_free_);
                readOptional(OCP_K_PSRAM_LARGEST, &probe_psram_largest_);
                probe_status_valid_ = true;
                last_status_reply_ms_ = now_;
            }
        }
    } else if (it.tag == OCP_MARK_STOP) {
        /* Clear only the lane the probe says it stopped (D-16). A pre-D-16
         * probe omits lane=, and stopped everything. */
        const auto *lane = it.get(OCP_K_LANE);
        bool all = !lane || *lane == OCP_LANE_ALL;
        if (all || *lane == OCP_LANE_PHY) {
            anti_start_pending_ = false;
            scan_.stop();
            contacts_.stopSniffing();
            spectrum_.stop();
            deauth_.stop();
            bt_.stop();
            anti_.stop();
            mesh_.stop();
        }
        if (all || *lane == OCP_LANE_LORA) {
            lora_.stop();
            storage::loraLogEnd();
        }
    } else if (it.tag == OCP_MARK_SNIFF) {
        log("sniffer started");
    } else if (it.tag == OCP_MARK_CLIENTS) {
        contacts_.absorbClients(it);
    } else if (it.tag == OCP_MARK_PROBES) {
        contacts_.absorbProbes(it);
    } else if (it.tag == OCP_MARK_CHAN) {
        log("channel_view started");
    } else if (it.tag == OCP_MARK_LORA) {
        /* Shared by lora_listen and lora_status; params are already known
         * locally (startLoraListen set them), same reasoning as OCP_MARK_CFG
         * below. lora_listen_pending_ (set only by startLoraListen, cleared
         * here or on error) is what tells the two apart, since this marker
         * alone doesn't say which verb it's answering. */
        if (lora_listen_pending_) {
            lora_listen_pending_ = false;
            lora_.begin();
            lora_cursor_ = 0;
            log(storage::loraLogBegin() ? "lora log: recording" : "lora log: sd unavailable, not recording this session");
        } else {
            log("lora reply");
        }
    } else if (it.tag == OCP_MARK_CFG) {
        /* Shared reply marker (packet_monitor and deauth_detector both use
         * it): which verb it's for is whatever we just sent, not decodable
         * from the frame itself — this is diagnostic-only, so "cfg" is fine. */
        const auto *ch = it.get(OCP_K_CH);
        log(std::string("cfg ack") + (ch ? " ch=" + *ch : std::string()));
    } else if (it.tag == OCP_MARK_SCAN) {
        next_page_ = scan_.absorbPage(it);   /* requested from tick(): not re-entrant here */
        logScanRows();
        const auto *first = it.get(OCP_K_FIRST);
        log("scan-page first=" + (first ? *first : std::string("?")) + " rows=" + std::to_string(it.rows.size()) +
            (next_page_ ? " next=" + std::to_string(next_page_) : std::string()));
        if (scan_.aborted()) {
            notice("scan stopped");
            log("scan aborted");
        } else if (!scan_.scanning()) {
            log("scan done aps=" + std::to_string(scan_.rows().size()) + " total=" + std::to_string(scan_.total()) +
                " malformed=" + std::to_string(scan_.malformedRows()) + " elapsed_ms=" + std::to_string(scan_.elapsedMs()));
            /* Wardrive mode (Drive card, `l`): a survey is only useful
             * repeated, not one-shot. Re-request immediately rather than
             * waiting to be told - the whole point is to sit passively and
             * watch the count climb. Screen-independent, same as every
             * other engine here: leaving Drive for SubGhz doesn't stop it. */
            if (storage::wardriveLogStats().open) {
                notice("");
                requestScan(now_);
            } else {
                notice(scan_.truncated() ? "list capped at 512" : "");
            }
        }
    } else if (it.tag == OCP_MARK_INSPECT) {
        scan_.absorbInspect(it);
        const auto &in = scan_.inspect();
        if (in.aborted) notice("inspect stopped");
        log("inspect idx=" + std::to_string(in.idx) + " beacons=" + std::to_string(in.beacons) +
            " rsn=" + std::to_string(in.rsn) + " mfp_capable=" + std::to_string(in.mfp_capable) +
            " mfp_required=" + std::to_string(in.mfp_required) + " aborted=" + std::to_string(in.aborted));
    } else if (it.tag == OCP_MARK_BLE) {
        bt_.absorbScan(it);
        log("ble scan devices=" + std::to_string(bt_.devices().size()) +
            " malformed=" + std::to_string(bt_.malformedRows()));
    } else if (it.tag == OCP_MARK_ZIG) {
        mesh_start_pending_ = false;
        mesh_.absorb(it);
        log("mesh pans=" + std::to_string(mesh_.pans().size()) + " nodes=" + std::to_string(mesh_.nodes().size()) +
            " malformed=" + std::to_string(mesh_.malformedRows()));
    }
}

void DeckApp::onEvent(const ocp::Item &it)
{
    dirty_ = true;
    contacts_.absorbEvent(it);   /* ticker only: [CLIENTS]/[PROBES] stay the authority */
    spectrum_.absorbEvent(it);   /* kind=chan is the only source of truth here, no dump verb */
    if (const auto *p = lora_.absorbEvent(it)) {   /* kind=lora is the only source of truth here too */
        storage::loraLogPacket(lora_.freqHz(), lora_.sf(), lora_.bwKhz(), lora_.cr(), *p);
    }
    deauth_.absorbEvent(it);     /* kind=deauth is the only source of truth here too */
    bt_.absorbEvent(it);         /* kind=airtag is the only source of truth here too */
    anti_.absorbEvent(it, now_); /* kind=airtag + deck-local movement correlation */
    scan_.absorbEvent(it);       /* kind=network: first-sighting AP discovery */
}

void DeckApp::requestScan(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) return;
    if (!client_.send(OCP_V_SCAN_NETWORKS, now_ms)) {
        retrySoon([this](uint32_t t) { requestScan(t); }, now_ms);
        return;
    }
    scan_.begin();
    next_page_ = 0;
    scan_started_ms_ = now_ms;
}

void DeckApp::startScan(uint32_t now_ms)
{
    if (scan_.continuousActive()) { notice("stop live scan first"); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    requestScan(now_ms);
    cursor_ = 0;
    screen_ = Screen::Sweep;
    notice("");
}

void DeckApp::toggleWifiContinuous(uint32_t now_ms)
{
    if (scan_.continuousActive()) { client_.stop(now_ms, OCP_LANE_PHY); return; }
    if (wifi_continuous_pending_) { notice("starting live scan..."); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_START_WIFI_SCAN, now_ms)) {
        retrySoon([this](uint32_t t) { toggleWifiContinuous(t); }, now_ms);
        return;
    }
    wifi_continuous_pending_ = true;
    scan_.beginContinuous();
    cursor_ = 0;
    scan_started_ms_ = now_ms;
    screen_ = Screen::Sweep;
    notice("");
}

void DeckApp::startInspect(uint32_t now_ms)
{
    if (scan_.rows().empty()) return;
    if (scan_.continuousActive()) { notice("stop live scan first"); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    trace_idx_ = scan_.rows()[cursor_].idx;
    if (!client_.send(std::string(OCP_V_INSPECT_NETWORK) + " " + std::to_string(trace_idx_), now_ms)) {
        retrySoon([this](uint32_t t) { startInspect(t); }, now_ms);
        return;
    }
    screen_ = Screen::Trace;
    notice("");
}

void DeckApp::startSniffer(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_START_SNIFFER, now_ms)) {
        retrySoon([this](uint32_t t) { startSniffer(t); }, now_ms);
        return;
    }
    contacts_.begin();
    contacts_cursor_ = 0;
    contacts_tab_ = ui::ContactsTab::Clients;
    contacts_poll_clients_ = true;
    last_contacts_poll_ms_ = now_ms;
    screen_ = Screen::Contacts;
    notice("");
}

void DeckApp::startChannelView(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_CHANNEL_VIEW, now_ms)) {
        retrySoon([this](uint32_t t) { startChannelView(t); }, now_ms);
        return;
    }
    spectrum_.begin();
    spectrum_cursor_ = 0;
    notice("");
}

void DeckApp::startPacketMonitor(uint32_t now_ms, uint8_t ch)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(std::string(OCP_V_PACKET_MONITOR) + " " + std::to_string(ch), now_ms)) {
        retrySoon([this, ch](uint32_t t) { startPacketMonitor(t, ch); }, now_ms);
        return;
    }
    spectrum_.beginLocked(ch);
    notice("");
}

namespace {
/* Placeholder until there's a real config UI (no numeric entry on this
 * keyboard yet): MeshCore's own USA/Canada preset, confirmed from their
 * docs, not a protocol-level default (ocp.h's lora_config takes no default
 * frequency on purpose — D-9, receive-only means no baked-in region plan). */
constexpr uint32_t kBenchFreqHz = 910525000;
constexpr int kBenchSf = 7;
constexpr int kBenchBwKhz = 62;
constexpr int kBenchCr = 1;
}  // namespace

void DeckApp::startLoraConfig(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    std::string cmd = std::string(OCP_V_LORA_CONFIG) + " " + std::to_string(kBenchFreqHz) + " " +
                       std::to_string(kBenchSf) + " " + std::to_string(kBenchBwKhz) + " " + std::to_string(kBenchCr);
    if (!client_.send(cmd, now_ms)) {
        retrySoon([this](uint32_t t) { startLoraConfig(t); }, now_ms);
        return;
    }
    lora_.configured(kBenchFreqHz, kBenchSf, kBenchBwKhz, kBenchCr);
    notice("");
}

void DeckApp::startLoraListen(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!lora_.hasConfig()) { notice("config first (c)"); return; }
    if (!client_.send(OCP_V_LORA_LISTEN, now_ms)) {
        retrySoon([this](uint32_t t) { startLoraListen(t); }, now_ms);
        return;
    }
    lora_listen_pending_ = true;   /* [LORA]/error reply decides whether to actually start (below) */
    notice("");
}

void DeckApp::startDeauthDetector(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_DEAUTH_DETECTOR, now_ms)) {
        retrySoon([this](uint32_t t) { startDeauthDetector(t); }, now_ms);
        return;
    }
    deauth_.begin();
    deauth_cursor_ = 0;
    notice("");
}

void DeckApp::startBtScan(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_SCAN_BT, now_ms)) {
        retrySoon([this](uint32_t t) { startBtScan(t); }, now_ms);
        return;
    }
    bt_.beginScan();
    bt_cursor_ = 0;
    notice("");
}

void DeckApp::toggleBtContinuous(uint32_t now_ms)
{
    if (bt_.continuousActive()) { client_.stop(now_ms, OCP_LANE_PHY); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_START_BLE_SCAN, now_ms)) {
        retrySoon([this](uint32_t t) { toggleBtContinuous(t); }, now_ms);
        return;
    }
    bt_.beginContinuous();
    bt_cursor_ = 0;
    notice("");
}

void DeckApp::toggleAirtagScan(uint32_t now_ms)
{
    if (bt_.airtagActive()) { client_.stop(now_ms, OCP_LANE_PHY); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_SCAN_AIRTAG, now_ms)) {
        retrySoon([this](uint32_t t) { toggleAirtagScan(t); }, now_ms);
        return;
    }
    bt_.beginAirtag();
    notice("");
}

void DeckApp::toggleAntisurveillance(uint32_t now_ms)
{
    if (anti_.active()) { client_.stop(now_ms, OCP_LANE_PHY); return; }
    if (anti_start_pending_) { notice("starting anti-surveillance..."); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_START_ANTISURV, now_ms)) {
        retrySoon([this](uint32_t t) { toggleAntisurveillance(t); }, now_ms);
        return;
    }
    anti_start_pending_ = true;
    bt_.resetTrackerLog();
    notice("starting anti-surveillance...");
}

void DeckApp::toggleMesh(uint32_t now_ms)
{
    if (mesh_.active()) { client_.stop(now_ms, OCP_LANE_PHY); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_START_ZIG_RECON, now_ms)) { retrySoon([this](uint32_t t) { toggleMesh(t); }, now_ms); return; }
    mesh_start_pending_ = true;
    mesh_.begin(); mesh_cursor_ = 0; screen_ = Screen::Mesh; notice("");
}

void DeckApp::feedGnss(const uint8_t *data, size_t len, uint32_t now_ms)
{
    now_ = now_ms;
    gnss_parser_.feed(data, len, [&](const gnss::Sentence &s) { gnss_.absorb(s, now_ms); });

    if (gnss_.hasFix() && now_ms - last_track_point_ms_ >= kTrackPointMs) {
        last_track_point_ms_ = now_ms;
        storage::wardriveLogTrackPoint(gnss_.fix());
    }
}

void DeckApp::toggleWardriveLog(uint32_t now_ms)
{
    if (storage::wardriveLogStats().open) {
        const auto &st = storage::wardriveLogStats();
        log("wardrive log end aps=" + std::to_string(st.aps) +
            " nofix=" + std::to_string(st.aps_no_fix) +
            " trk=" + std::to_string(st.track_points));
        storage::wardriveLogEnd();
        notice("log closed");
        return;
    }

    if (!storage::wardriveLogBegin(gnss_.fix())) {
        notice("no SD: logging unavailable");
        log("wardrive log begin failed");
        return;
    }
    /* Whatever the current scan already holds predates the session; only
     * rows seen from here on belong to it. */
    wardrive_logged_upto_ = scan_.rows().size();
    notice(std::string("logging ") + storage::wardriveLogName());
    log("wardrive log begin " + std::string(storage::wardriveLogName()));

    /* A wardrive session that just sits there until you separately go to
     * Sweep isn't a wardrive mode, it's a hook - kick a survey off now, and
     * the SCAN handler keeps it looping (onReply(), OCP_MARK_SCAN) for as
     * long as the log stays open. Skipped if one's already running: let it
     * finish rather than restarting it and losing its progress. */
    if (!scan_.scanning()) requestScan(now_ms);
}

void DeckApp::logScanRows()
{
    if (!storage::wardriveLogStats().open) return;

    const auto &rows = scan_.rows();
    /* A fresh scan restarts the table, so a shrunken row count means the
     * watermark is stale, not that rows vanished. */
    if (wardrive_logged_upto_ > rows.size()) wardrive_logged_upto_ = 0;

    for (size_t i = wardrive_logged_upto_; i < rows.size(); i++) {
        storage::wardriveLogAp(rows[i], gnss_.fix(), gnss_.fixAgeMs(now_));
    }
    wardrive_logged_upto_ = rows.size();
}

void DeckApp::back(uint32_t now_ms)
{
    /* A live sniffer/spectrum mode has no pending command once its ack
     * lands (it's a stream, not a blocking reply), so leaving the screen
     * must send `stop` unconditionally rather than only when something is
     * pending. Scoped per D-16: SubGhz is the LoRa lane, the other three
     * are the PHY lane — a bare stop here would cancel whichever of the
     * two isn't actually being left, same bug D-16 fixed at the protocol
     * layer, just reachable again if this call didn't scope it too. */
    if (screen_ == Screen::SubGhz) {
        client_.stop(now_ms, OCP_LANE_LORA);
    } else if (screen_ == Screen::Sweep || screen_ == Screen::Contacts || screen_ == Screen::Spectrum || screen_ == Screen::Mesh || screen_ == Screen::Deauth || screen_ == Screen::Beacons) {
        client_.stop(now_ms, OCP_LANE_PHY);
    } else if (client_.pending()) {
        /* Only Sweep/Trace/Link reach here with something pending, and
         * every command they can issue is PHY-lane (scan/inspect) or
         * lane-agnostic (status/ping/version) - "all" is exact, not just
         * a safe default. */
        client_.stop(now_ms);
    }
    screen_ = screen_ == Screen::Trace ? Screen::Sweep : Screen::Link;
    dirty_ = true;
}

void DeckApp::onKeys(const Keys &keys, uint32_t now_ms)
{
    now_ = now_ms;
    if (!keys.chars.empty() || keys.enter) log("keys=" + keys.chars + (keys.enter ? "<enter>" : ""));
    for (char c : keys.chars) {
        if (c == '`') { back(now_ms); continue; }
        if (c == 'd') { toggleDebugMode(); continue; }
        if ((c == ',' || c == '/') && isHomeCard(screen_)) {
            screen_ = cycleCard(screen_, c == '/' ? 1 : -1);
            dirty_ = true;
            continue;
        }

        switch (screen_) {
        case Screen::Link:
            if (c == 'w') startScan(now_ms);
            else if (c == 'h') client_.connect(now_ms);
            else if (c == 'p') client_.send(OCP_V_PING, now_ms);
            else if (c == 's') client_.send(OCP_V_STATUS, now_ms);
            else if (c == 'r') client_.send(OCP_V_REBOOT, now_ms);
            break;
        case Screen::Sweep:
            if (c == ';' && cursor_ > 0) cursor_--;
            else if (c == '.' && cursor_ + 1 < scan_.rows().size()) cursor_++;
            else if (c == 'r') startScan(now_ms);
            else if (c == 'c') toggleWifiContinuous(now_ms);
            break;
        case Screen::Trace:
            if (c == 'i') startInspect(now_ms);
            break;
        case Screen::Contacts: {
            size_t n = contacts_tab_ == ui::ContactsTab::Clients ? contacts_.clients().size()
                                                                  : contacts_.probes().size();
            if (c == ';' && contacts_cursor_ > 0) contacts_cursor_--;
            else if (c == '.' && contacts_cursor_ + 1 < n) contacts_cursor_++;
            else if (c == 'x') {
                contacts_tab_ = contacts_tab_ == ui::ContactsTab::Clients ? ui::ContactsTab::Probes
                                                                          : ui::ContactsTab::Clients;
                contacts_cursor_ = 0;
            } else if (c == 's') {
                if (contacts_.sniffing()) client_.stop(now_ms, OCP_LANE_PHY);
                else startSniffer(now_ms);
            }
            break;
        }
        case Screen::Info:
            break;   /* nothing but card-cycling and back here */
        case Screen::Spectrum:
            if (c == ';' && spectrum_cursor_ > 0) spectrum_cursor_--;
            else if (c == '.' && spectrum_cursor_ + 1 < spectrum_.readings().size()) spectrum_cursor_++;
            else if (c == 's') {
                if (spectrum_.active()) client_.stop(now_ms, OCP_LANE_PHY);
                else startChannelView(now_ms);
            }
            break;
        case Screen::Mesh:
            if (c == ';' && mesh_cursor_ > 0) mesh_cursor_--;
            else if (c == '.' && mesh_cursor_ + 1 < mesh_.nodes().size()) mesh_cursor_++;
            else if (c == 's') toggleMesh(now_ms);
            break;
        case Screen::SubGhz:
            if (c == ';' && lora_cursor_ > 0) lora_cursor_--;
            else if (c == '.' && lora_cursor_ + 1 < lora_.packets().size()) lora_cursor_++;
            else if (c == 'c') startLoraConfig(now_ms);
            else if (c == 's') {
                if (lora_.active()) client_.stop(now_ms, OCP_LANE_LORA);
                else startLoraListen(now_ms);
            }
            break;
        case Screen::Deauth:
            if (c == ';' && deauth_cursor_ > 0) deauth_cursor_--;
            else if (c == '.' && deauth_cursor_ + 1 < deauth_.events().size()) deauth_cursor_++;
            else if (c == 's') {
                if (deauth_.active()) client_.stop(now_ms, OCP_LANE_PHY);
                else startDeauthDetector(now_ms);
            }
            break;
        case Screen::Drive:
            if (c == 'l') toggleWardriveLog(now_ms);
            break;
        case Screen::Beacons:
            if (c == ';' && bt_cursor_ > 0) bt_cursor_--;
            else if (c == '.' && bt_cursor_ + 1 < bt_.devices().size()) bt_cursor_++;
            else if (c == 's') startBtScan(now_ms);
            else if (c == 'c') toggleBtContinuous(now_ms);
            else if (c == 'a') toggleAirtagScan(now_ms);
            else if (c == 'f') toggleAntisurveillance(now_ms);
            break;
        }
        dirty_ = true;
    }
    if (keys.enter && screen_ == Screen::Sweep && !scan_.scanning() && !scan_.continuousActive()) startInspect(now_ms);
    if (keys.enter && screen_ == Screen::Spectrum && !spectrum_.locked() &&
        spectrum_cursor_ < spectrum_.readings().size()) {
        startPacketMonitor(now_ms, spectrum_.readings()[spectrum_cursor_].ch);
    }
}

void DeckApp::toggleDebugMode()
{
    debug_mode_ = !debug_mode_;
    storage::saveDebugMode(debug_mode_);
    notice(debug_mode_ ? "debug mode on" : "debug mode off");
    log(std::string("debug mode ") + (debug_mode_ ? "on" : "off"));
    dirty_ = true;
}

void DeckApp::runDebugCommand(const std::string &line, uint32_t now_ms)
{
    if (!debug_mode_) return;
    if (line.empty()) return;

    std::string cmd = line;
    std::string arg;
    size_t sp = line.find(' ');
    if (sp != std::string::npos) {
        cmd = line.substr(0, sp);
        arg = line.substr(sp + 1);
    }

    /* Direct calls into the same actions onKeys() reaches, deliberately
     * skipping the screen_/cursor_ gating those go through — a script
     * driving this shouldn't have to navigate the UI first. Toggle-shaped
     * commands (sniff/spectrum/lora/deauth) read the same active()/
     * sniffing()/locked() state their `s`-key equivalents do, so a script
     * doesn't need to track which state it left an engine in either. */
    if (cmd == "scan") startScan(now_ms);
    else if (cmd == "wardrive") toggleWardriveLog(now_ms);
    else if (cmd == "connect") client_.connect(now_ms);
    else if (cmd == "ping") client_.send(OCP_V_PING, now_ms);
    else if (cmd == "status") client_.send(OCP_V_STATUS, now_ms);
    else if (cmd == "reboot") client_.send(OCP_V_REBOOT, now_ms);
    else if (cmd == "stop") client_.stop(now_ms, arg.empty() ? OCP_LANE_ALL : arg);
    else if (cmd == "sniff") { if (contacts_.sniffing()) client_.stop(now_ms, OCP_LANE_PHY); else startSniffer(now_ms); }
    else if (cmd == "spectrum") { if (spectrum_.active()) client_.stop(now_ms, OCP_LANE_PHY); else startChannelView(now_ms); }
    else if (cmd == "deauth") { if (deauth_.active()) client_.stop(now_ms, OCP_LANE_PHY); else startDeauthDetector(now_ms); }
    else if (cmd == "wifiscan") toggleWifiContinuous(now_ms);
    else if (cmd == "beacons") startBtScan(now_ms);
    else if (cmd == "blescan") toggleBtContinuous(now_ms);
    else if (cmd == "airtag") toggleAirtagScan(now_ms);
    else if (cmd == "antisurv") toggleAntisurveillance(now_ms);
    else if (cmd == "lora") {
        if (arg == "config") startLoraConfig(now_ms);
        else if (lora_.active()) client_.stop(now_ms, OCP_LANE_LORA);
        else startLoraListen(now_ms);
    }
    else if (cmd == "channel") {
        if (arg.empty()) { log("debug: channel needs a number"); return; }
        uint64_t ch = 0;
        if (!model::parseUnsigned(arg, &ch) || ch > 255) { log("debug: channel is invalid"); return; }
        startPacketMonitor(now_ms, static_cast<uint8_t>(ch));
    }
    else if (cmd == "inspect") {
        if (!arg.empty()) {
            uint64_t idx = 0;
            if (!model::parseUnsigned(arg, &idx) || idx > UINT16_MAX) {
                log("debug: index is invalid"); return;
            }
            bool found = false;
            for (size_t i = 0; i < scan_.rows().size(); i++) {
                if (scan_.rows()[i].idx == idx) { cursor_ = i; found = true; break; }
            }
            if (!found) { log("debug: no scan row idx=" + arg); return; }
        }
        startInspect(now_ms);
    }
    else if (cmd == "card") {
        Screen s;
        if (!screenFromName(arg, &s)) { log("debug: unknown card=" + arg); return; }
        screen_ = s;
        dirty_ = true;
    }
    else if (cmd == "dump") {
        /* Counts and states only, same rule as every other log() call
         * (deck_app.h) — this is a stability/regression snapshot, not a
         * capture tool. */
        log("dump link=" + std::string(ocp::linkStateName(client_.state())) +
            " scan_rows=" + std::to_string(scan_.rows().size()) +
            " clients=" + std::to_string(contacts_.clients().size()) +
            " probes=" + std::to_string(contacts_.probes().size()) +
            " spectrum=" + std::to_string(spectrum_.readings().size()) +
            " lora_pkts=" + std::to_string(lora_.packets().size()) +
            " deauth_evt=" + std::to_string(deauth_.events().size()) +
            " bt_devices=" + std::to_string(bt_.devices().size()) +
            " trackers=" + std::to_string(bt_.trackerCount()) +
            " anti_alerts=" + std::to_string(anti_.alertCount()) +
            " anti_starting=" + std::string(anti_start_pending_ ? "1" : "0") +
            " probe_heap=" + std::to_string(probe_heap_) +
            " probe_heap_min=" + std::to_string(probe_heap_min_) +
            " probe_heap_largest=" + std::to_string(probe_heap_largest_) +
            " probe_psram_total=" + std::to_string(probe_psram_total_) +
            " probe_psram_free=" + std::to_string(probe_psram_free_) +
            " probe_psram_largest=" + std::to_string(probe_psram_largest_) +
            " gnss=" + std::string(model::gnssStateName(gnss_.state(now_ms))) +
            " wardrive_open=" + std::string(storage::wardriveLogStats().open ? "1" : "0"));
        return;
    }
    else { log("debug: unknown cmd=" + cmd); return; }

    log("debug: ok " + line);
}

void DeckApp::tick(uint32_t now_ms)
{
    now_ = now_ms;
    client_.tick(now_ms);

    /* Client timeouts intentionally have no callback. Clean up the optimistic
     * UI state here so a lost/invalid CFG cannot leave anti-surveillance
     * visibly stuck in "starting". */
    if (anti_start_pending_ && !client_.pending()) {
        anti_start_pending_ = false;
        anti_.stop();
        notice("anti-surveillance start timed out");
        log("anti start timeout");
    }

    if (pending_retry_) {
        if (!client_.pending()) {
            auto action = std::move(pending_retry_);
            pending_retry_ = nullptr;
            log("retry firing (pending cleared)");
            action(now_ms);   /* may itself queue another retry if still busy */
        } else if (now_ms - pending_retry_started_ms_ >= kPendingRetryWindowMs) {
            pending_retry_ = nullptr;
            notice("busy (gave up)");
            log("retry gave up after " + std::to_string(kPendingRetryWindowMs) + "ms");
        }
    }

    if (next_page_ && !client_.pending()) {
        uint16_t first = next_page_;
        next_page_ = 0;
        if (!client_.send(std::string(OCP_V_SHOW_SCAN_RESULTS) + " " + std::to_string(first), now_ms)) {
            next_page_ = first;   /* retry next tick */
        }
    }

    auto st = client_.state();
    if (st == ocp::LinkState::Disconnected && now_ms - last_attempt_ms_ >= kRetryMs) {
        last_attempt_ms_ = now_ms;
        client_.connect(now_ms);
    }
    if (screen_ == Screen::Contacts && st == ocp::LinkState::Ready && !client_.pending() &&
        now_ms - last_contacts_poll_ms_ >= kContactsPollMs) {
        last_contacts_poll_ms_ = now_ms;
        const char *verb = contacts_poll_clients_ ? OCP_V_SHOW_CLIENTS : OCP_V_SHOW_PROBES;
        contacts_poll_clients_ = !contacts_poll_clients_;
        client_.send(verb, now_ms);
    }
    if (screen_ == Screen::Info && st == ocp::LinkState::Ready && !client_.pending() &&
        now_ms - last_status_poll_ms_ >= kInfoPollMs) {
        last_status_poll_ms_ = now_ms;
        client_.send(OCP_V_STATUS, now_ms);
    }
    if (screen_ == Screen::Mesh && mesh_.active() && st == ocp::LinkState::Ready && !client_.pending() &&
        now_ms - last_mesh_poll_ms_ >= kMeshPollMs) {
        last_mesh_poll_ms_ = now_ms;
        client_.send(OCP_V_ZIG_LIST, now_ms);
    }
    if (st == ocp::LinkState::Ready && !client_.pending() && !next_page_ &&
        now_ms - last_keepalive_ms_ >= kKeepaliveMs) {
        last_keepalive_ms_ = now_ms;
        client_.send(OCP_V_PING, now_ms);
    }

    if (anti_.active()) {
        anti_.observePosition(gnss_.fix().lat_deg, gnss_.fix().lon_deg,
                              gnss_.state(now_ms) == model::GnssState::Fixed,
                              gnss_.fixAgeMs(now_ms), now_ms);
    }

    /* Distinguishes a marginal fix from GNSS bytes being lost in transit
     * (2026-09-16, WORKLOG): chkfail/overlong are wire corruption, age_ms is
     * how stale the last good fix is regardless of cause. */
    if (now_ms - last_gnss_diag_ms_ >= kGnssDiagMs) {
        last_gnss_diag_ms_ = now_ms;
        log("gnss " + std::string(model::gnssStateName(gnss_.state(now_ms))) +
            " age_ms=" + std::to_string(gnss_.fixAgeMs(now_ms)) +
            " chkfail=" + std::to_string(gnss_parser_.checksumFailures()) +
            " overlong=" + std::to_string(gnss_parser_.overlongLines()));
    }
}

bool DeckApp::dirty(uint32_t now_ms) const
{
    if (now_ms - last_draw_ms_ < kRedrawMs) return false;
    bool busy = scan_.scanning() || scan_.continuousActive() || (screen_ == Screen::Trace && client_.pending());
    return dirty_ || (busy && now_ms - last_draw_ms_ >= kBusyRedrawMs);
}

void DeckApp::draw(uint32_t now_ms)
{
    switch (screen_) {
    case Screen::Link:
        ui::drawLinkView(client_, last_reply_, notice_, debug_mode_);
        break;
    case Screen::Sweep:
        ui::drawSweepView(scan_, cursor_, now_ms - scan_started_ms_, notice_);
        break;
    case Screen::Trace: {
        const model::ApRow *row = nullptr;
        for (const auto &r : scan_.rows()) if (r.idx == trace_idx_) row = &r;
        /* Only this AP's result, and only an inspect counts as listening:
         * a keepalive ping is pending too. */
        static const model::Inspect none;
        const auto &in = scan_.inspect().idx == trace_idx_ ? scan_.inspect() : none;
        bool listening = client_.pending() && client_.pendingVerb() == OCP_V_INSPECT_NETWORK;
        ui::drawTraceView(row, in, listening, notice_);
        break;
    }
    case Screen::Contacts:
        ui::drawContactsView(contacts_, contacts_tab_, contacts_cursor_, notice_);
        break;
    case Screen::Info:
        ui::drawInfoView(client_, probe_status_valid_, probe_heap_, probe_uptime_ms_,
                         now_ms - last_status_reply_ms_, notice_);
        break;
    case Screen::Spectrum:
        ui::drawSpectrumView(spectrum_, spectrum_cursor_, notice_);
        break;
    case Screen::Mesh:
        ui::drawMeshView(mesh_, mesh_cursor_, notice_);
        break;
    case Screen::SubGhz:
        ui::drawSubGhzView(lora_, lora_cursor_, notice_);
        break;
    case Screen::Deauth:
        ui::drawDeauthView(deauth_, deauth_cursor_, notice_);
        break;
    case Screen::Drive:
        ui::drawGnssView(gnss_, now_ms, notice_);
        break;
    case Screen::Beacons:
        ui::drawBtView(bt_, anti_, bt_cursor_, notice_);
        break;
    }
    ui::present();
    dirty_ = false;
    last_draw_ms_ = now_ms;
}

}  // namespace app
