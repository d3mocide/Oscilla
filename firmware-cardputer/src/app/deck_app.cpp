/*
 * deck_app.cpp — see deck_app.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/deck_app.h"

#include <cstdio>
#include <cstdlib>

#include <M5Cardputer.h>

#include "app/deck_navigation.h"
#include "ocp.h"
#include "ui/anti_surveillance_view.h"
#include "ui/bt_view.h"
#include "ui/canvas.h"
#include "ui/contacts_view.h"
#include "ui/deauth_view.h"
#include "ui/help_view.h"
#include "storage/lora_logger.h"
#include "storage/sd_storage.h"
#include "storage/settings.h"
#include "storage/wardrive_logger.h"
#include "model/lora_profiles.h"
#include "model/number_parse.h"
#include "ui/gnss_view.h"
#include "ui/info_view.h"
#include "ui/link_view.h"
#include "ui/zig_view.h"
#include "ui/settings_view.h"
#include "ui/spectrum_view.h"
#include "ui/subghz_view.h"
#include "ui/lora_detail_view.h"
#include "ui/contact_detail_view.h"
#include "ui/wifi_detail_view.h"
#include "ui/bt_detail_view.h"
#include "ui/session_view.h"
#include "storage/session_reader.h"
#include "ui/sweep_view.h"

namespace app {

namespace {
constexpr uint32_t kRetryMs = 2000;       /* reconnect while Disconnected */
constexpr uint32_t kKeepaliveMs = 3000;   /* ping when idle, to notice a lost probe */
constexpr uint32_t kRedrawMs = 200;
constexpr uint32_t kBusyRedrawMs = 500;   /* elapsed counter while scanning */
constexpr uint32_t kContactsPollMs = 1500;   /* [CLIENTS]/[PROBES] are the authority, events are just a ticker */
constexpr uint32_t kZigPollMs = 1500;       /* [ZIG] table is authoritative; events are only first-sighting hints */
constexpr uint32_t kInfoPollMs = 2000;       /* how often to refresh the probe's heap/uptime */
constexpr uint32_t kLoraHealthPollMs = 10000; /* bounded session evidence, not a packet-rate meter */
constexpr uint32_t kBatteryPollMs = 4000;    /* M5.Power is an I2C read; battery barely moves */
constexpr uint32_t kNoticeDurationMs = 2000; /* transient toast lifetime */
constexpr uint32_t kPendingRetryWindowMs = 4000;   /* generous vs. any single command's own reply timeout */
/* Drive-track vertex cadence. A 1 Hz fix logged raw is 3600 points an hour
 * for a line that only needs enough shape to draw. */
constexpr uint32_t kTrackPointMs = 5000;
/* GNSS diagnostic cadence (2026-09-16, see WORKLOG): tight enough to watch
 * live over USB serial during a bench test, not so tight it floods it. */
constexpr uint32_t kGnssDiagMs = 5000;

/* Debug console only (`card <name>`) — the keyboard never needs this, it
 * reaches Wi-Fi Scan/AP Detail by drilling down instead (DESIGN §7.3). */
bool screenFromName(const std::string &name, Screen *out)
{
    if (name == "link") *out = Screen::Link;
    else if (name == "sweep" || name == "scan" || name == "wifi" || name == "wifi_scan") *out = Screen::Sweep;
    else if (name == "trace" || name == "inspect" || name == "ap_detail") *out = Screen::Trace;
    else if (name == "contacts" || name == "sniffer") *out = Screen::Contacts;
    else if (name == "info" || name == "system") *out = Screen::Info;
    else if (name == "spectrum" || name == "packet_monitor" || name == "monitor") *out = Screen::Spectrum;
    else if (name == "zig" || name == "802154" || name == "zigbee") *out = Screen::Zig;
    else if (name == "subghz" || name == "lora" || name == "lora_rx") *out = Screen::SubGhz;
    else if (name == "deauth" || name == "deauth_detect") *out = Screen::Deauth;
    else if (name == "anti" || name == "antisurv" || name == "anti_surveillance") *out = Screen::AntiSurveillance;
    else if (name == "logs" || name == "sessions") *out=Screen::Logs;
    else if (name == "drive" || name == "wardrive") *out = Screen::Drive;
    else if (name == "beacons" || name == "ble" || name == "ble_scan") *out = Screen::Beacons;
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
            ++probe_status_epoch_;
            phy_handoff_.clear();
            lora_listen_pending_ = false;
            lora_config_pending_ = false;
            scan_pending_ = false;
            wifi_continuous_pending_ = false;
            zig_start_pending_ = false;
            anti_start_pending_ = false;
            bt_scan_pending_ = false;
            bt_continuous_pending_ = false;
            bt_airtag_pending_ = false;
            spectrum_start_pending_ = false;
            anti_.stop();
            bt_.stop();
            spectrum_.stop();
            probe_status_valid_ = false;
            probe_region_.clear();
            region_push_pending_ = false;
            probe_heap_ = 0;
            probe_heap_total_ = 0;
            probe_uptime_ms_ = 0;
            /* Same reasoning: a queued retry is moot once the link isn't
             * Ready, and must never fire later against something unrelated
             * after a reconnect. */
            pending_retry_ = nullptr;
        } else if (const char *lane = reconcileStopLane(phyToolActive(),
                                                        lora_.active() || lora_listen_pending_)) {
            /* Stop leftovers from before a deck reboot or link drop. */
            client_.stop(now_, lane);
        }
        if (s == ocp::LinkState::Ready) {
            if (!client_.probe().rst.empty()) log("probe rst=" + client_.probe().rst);
            probe_region_ = client_.probe().region;
            region_push_attempted_ = false;
            /* The probe may have rebooted while we were away: its status wins. */
            reconcile_on_status_ = client_.send(OCP_V_STATUS, now_);
        }
        dirty_ = true;
    });
    client_.onReset([this] {
        phy_handoff_.clear();
        /* The probe's stored results died with it; so did our indices. */
        const auto &st = client_.stats();
        log("probe-reset resets=" + std::to_string(st.resets) + " timeouts=" + std::to_string(st.timeouts) +
            " noise=" + std::to_string(st.noise) + " stray=" + std::to_string(st.stray));
        /* This unsolicited-reset shape (e.g. the zig/coexistence-defect
         * restart, D-18) always strands a pending command and reads a
         * burst of boot chatter as noise/stray on the way back up —
         * expected, not an ongoing link problem, so it shouldn't inflate
         * the counters a later genuine timeout gets judged against. The
         * line above already logged the pre-clear values for this event;
         * Client::handleHello() separately clears the same counters on its
         * own for the more common plain-reconnect shape, which never runs
         * this callback at all (see WORKLOG). */
        client_.clearTransientStats();
        scan_.clear();
        contacts_.clear();
        contact_inspection_.close();
        wifi_inspection_.close();bt_inspection_.close();
        spectrum_.clear();
        probe_status_valid_ = false;
        probe_heap_ = 0;
        probe_heap_total_ = 0;
        probe_uptime_ms_ = 0;
        lora_.clear();
        storage::loraLogEnd();
        deauth_.clear();
        bt_.clear();
        anti_.clear();
        zig_.clear();
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
    notice_started_ms_ = text.empty() ? 0 : now_;
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

    if (it.kind == ocp::ItemKind::Error && region_push_pending_) {
        /* Usually `busy`: the next idle moment retries (kRegionRetryMs). */
        region_push_pending_ = false;
        const auto *code = it.get(OCP_K_CODE);
        log(std::string("set_region refused code=") + (code ? *code : "?"));
        return;
    }
    if (it.kind == ocp::ItemKind::Error) {
        lora_listen_pending_ = false;   /* rejected: no session, no file (see deck_app.h) */
        lora_config_pending_ = false;   /* rejected: probe and model both keep the previous config */
        if (wifi_continuous_pending_) {
            wifi_continuous_pending_ = false;
            scan_.stop();
        }
        if (zig_start_pending_) {
            zig_start_pending_ = false;
            zig_.stop();
        }
        if (anti_start_pending_) {
            anti_start_pending_ = false;
            anti_.stop();
        }
        const bool spectrum_rejected = spectrum_start_pending_;
        spectrum_start_pending_ = false;
        if (spectrum_rejected) spectrum_.clear();
        const bool scan_rejected = scan_pending_;
        scan_pending_ = false;
        if (scan_rejected) {
            next_page_ = 0;
            scan_.clear();
        }
        const bool bt_scan_rejected = bt_scan_pending_;
        const bool bt_continuous_rejected = bt_continuous_pending_;
        const bool bt_airtag_rejected = bt_airtag_pending_;
        bt_scan_pending_ = false;
        bt_continuous_pending_ = false;
        bt_airtag_pending_ = false;
        if (bt_scan_rejected) bt_.stop();
        const auto *code = it.get(OCP_K_CODE);
        const auto *msg = it.get(OCP_K_MSG);
        if (code && *code == OCP_ERR_BUSY &&
            (bt_scan_rejected || bt_continuous_rejected || bt_airtag_rejected || spectrum_rejected)) {
            notice("PHY busy - stop current tool first");
        } else {
            notice(std::string("error ") + (code ? *code : "?") + ": " + (msg ? *msg : ""));
        }
        log(std::string("err code=") + (code ? *code : "?"));
        return;
    }

    last_reply_ = it.tag;
    if (it.tag == OCP_MARK_CFG && region_push_pending_) {
        region_push_pending_ = false;
        region_push_attempted_ = false;   /* landed: a newer pick may go at once */
        const auto *region = it.get(OCP_K_REGION);
        if (region) probe_region_ = *region;
        log("wifi region applied " + probe_region_);
        if (region_notice_pending_ && probe_region_ == wifi_region_) {
            region_notice_pending_ = false;
            char summary[64];
            ocp_wifi_region_summary(&app::regionOrDefault(wifi_region_), summary, sizeof summary);
            notice("wifi region " + wifi_region_ + ": ch " + summary);
        }
    }
    if (it.tag == OCP_MARK_CFG && lora_config_pending_) {
        lora_config_pending_ = false;
        const auto &c = lora_config_sent_;
        lora_.configured(c.freq_hz, c.sf, c.bw_khz, c.cr, c.sync_word, c.profile_token);
        notice("");
    }
    if (it.tag == OCP_MARK_BLE && bt_scan_pending_) {
        bt_scan_pending_ = false;
    }
    if (it.tag == OCP_MARK_CFG && wifi_continuous_pending_) {
        wifi_continuous_pending_ = false;
    }
    if (it.tag == OCP_MARK_CFG && bt_continuous_pending_) {
        bt_continuous_pending_ = false;
        bt_.beginContinuous();
        bt_cursor_ = 0;
        notice("");
    }
    if (it.tag == OCP_MARK_CFG && bt_airtag_pending_) {
        bt_airtag_pending_ = false;
        bt_.beginAirtag();
        notice("");
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
                readOptional(OCP_K_HEAP_TOTAL, &probe_heap_total_);
                readOptional(OCP_K_HEAP_MIN, &probe_heap_min_);
                readOptional(OCP_K_HEAP_LARGEST, &probe_heap_largest_);
                readOptional(OCP_K_PSRAM_TOTAL, &probe_psram_total_);
                readOptional(OCP_K_PSRAM_FREE, &probe_psram_free_);
                readOptional(OCP_K_PSRAM_LARGEST, &probe_psram_largest_);
                const auto *owner=it.get(OCP_K_OWNER), *lora=it.get(OCP_LANE_LORA);
                probe_owner_=owner?*owner:std::string{};
                probe_lora_=lora?*lora:std::string{};
                const auto *region=it.get(OCP_K_REGION);
                probe_region_=region?*region:std::string{};
                ++probe_status_sequence_;
                probe_status_valid_ = true;
                last_status_reply_ms_ = now_;
                if (reconcile_on_status_) {
                    reconcile_on_status_ = false;
                    const auto clear = reconcileFromStatus(phyEngineActive(), lora_.active(),
                                                           probe_owner_, probe_lora_);
                    if (clear.phy) clearPhyBelief();
                    if (clear.lora) clearLoraBelief();
                    if (clear.phy || clear.lora) notice("probe restarted - modes stopped");
                }
            }
        }
    } else if (it.tag == OCP_MARK_STOP) {
        /* Clear only the lane the probe says it stopped (D-16). A pre-D-16
         * probe omits lane=, and stopped everything. */
        const auto *lane = it.get(OCP_K_LANE);
        bool all = !lane || *lane == OCP_LANE_ALL;
        if (all || *lane == OCP_LANE_PHY) clearPhyBelief();
        if (all || *lane == OCP_LANE_LORA) clearLoraBelief();
        if (all || *lane == OCP_LANE_PHY) {
            PhyHandoff::Start start;
            if (phy_handoff_.takeAfterStop(&start)) start(now_);
        }
    } else if (it.tag == OCP_MARK_SNIFF) {
        log("sniffer started");
    } else if (it.tag == OCP_MARK_CLIENTS) {
        absorbContactSnapshot(it);
    } else if (it.tag == OCP_MARK_PROBES) {
        absorbContactSnapshot(it);
    } else if (it.tag == OCP_MARK_CHAN) {
        if (spectrum_start_pending_) {
            spectrum_start_pending_ = false;
            spectrum_.begin();
            spectrum_cursor_ = 0;
            notice("");
        }
        log("channel_view started");
    } else if (it.tag == OCP_MARK_LORA) {
        /* Shared by lora_listen and lora_status; params are already known
         * locally (startLoraListen set them), same reasoning as OCP_MARK_CFG
         * below. lora_listen_pending_ (set only by startLoraListen, cleared
         * here or on error) is what tells the two apart, since this marker
         * alone doesn't say which verb it's answering. */
        const bool health_updated = lora_.absorbStatus(it);
        if (health_updated) storage::loraLogHealth(lora_.health());
        if (lora_listen_pending_) {
            lora_listen_pending_ = false;
            lora_.begin();
            lora_cursor_ = 0;
            log(storage::loraLogBegin(lora_) ? "lora log: recording" : "lora log: sd unavailable, not recording this session");
        } else if (!health_updated) {
            log("lora reply");   /* routine 10s health polls stay quiet */
        }
    } else if (it.tag == OCP_MARK_CFG) {
        /* Shared reply marker (packet_monitor and deauth_detector both use
         * it): which verb it's for is whatever we just sent, not decodable
         * from the frame itself — this is diagnostic-only, so "cfg" is fine. */
        const auto *ch = it.get(OCP_K_CH);
        log(std::string("cfg ack") + (ch ? " ch=" + *ch : std::string()));
    } else if (it.tag == OCP_MARK_SCAN) {
        const size_t before=scan_.rows().size();
        next_page_ = absorbWifiPage(it);   /* requested from tick(): not re-entrant here */
        if(storage::wardriveLogStats().open && scan_.rows().size()>before){
            wardrive_page_fix_=gnss_.fix();
            const uint32_t age=gnss_.fixAgeMs(now_);
            wardrive_page_fix_.valid=gnss_.state(now_)==model::GnssState::Fixed && age<3000;
            wardrive_page_fix_ms_=now_-age;
        }
        const auto *first = it.get(OCP_K_FIRST);
        log("scan-page first=" + (first ? *first : std::string("?")) + " rows=" + std::to_string(it.rows.size()) +
            (next_page_ ? " next=" + std::to_string(next_page_) : std::string()));
        if (scan_.aborted()) {
            scan_pending_ = false;
            notice("scan stopped");
            log("scan aborted");
        } else if (!scan_.scanning()) {
            scan_pending_ = false;
            log("scan done aps=" + std::to_string(scan_.rows().size()) + " total=" + std::to_string(scan_.total()) +
                " malformed=" + std::to_string(scan_.malformedRows()) + " elapsed_ms=" + std::to_string(scan_.elapsedMs()));
            /* Drain this page before the next survey (DESIGN §9.2). */
            if (storage::wardriveLogStats().open && !wardrive_closing_) {
                notice("");
                wardrive_rescan_=true;
            } else {
                notice(scan_.truncated() ? "list capped at 512" : "");
            }
        }
    } else if (it.tag == OCP_MARK_INSPECT) {
        scan_.absorbInspect(it);
        wifi_inspection_.accept(scan_.inspect(),now_);
        const auto &in = scan_.inspect();
        if (in.aborted) notice("inspect stopped");
        log("inspect idx=" + std::to_string(in.idx) + " beacons=" + std::to_string(in.beacons) +
            " rsn=" + std::to_string(in.rsn) + " mfp_capable=" + std::to_string(in.mfp_capable) +
            " mfp_required=" + std::to_string(in.mfp_required) + " aborted=" + std::to_string(in.aborted));
    } else if (it.tag == OCP_MARK_BLE) {
        absorbBtSnapshot(it);
        log("ble scan devices=" + std::to_string(bt_.devices().size()) +
            " malformed=" + std::to_string(bt_.malformedRows()));
    } else if (it.tag == OCP_MARK_ZIG) {
        zig_start_pending_ = false;
        zig_.absorb(it);
        log("zig pans=" + std::to_string(zig_.pans().size()) + " nodes=" + std::to_string(zig_.nodes().size()) +
            " malformed=" + std::to_string(zig_.malformedRows()));
    }
}

void DeckApp::onEvent(const ocp::Item &it)
{
    dirty_ = true;
    contacts_.absorbEvent(it);   /* ticker only: [CLIENTS]/[PROBES] stay the authority */
    spectrum_.absorbEvent(it, now_);   /* kind=chan is the only source of truth here, no dump verb */
    if (const auto *p = lora_.absorbEvent(it, now_)) {   /* kind=lora is the only source of truth here too */
        storage::loraLogPacket(lora_.freqHz(), lora_.sf(), lora_.bwKhz(), lora_.cr(), *p);
    }
    deauth_.absorbEvent(it);     /* kind=deauth is the only source of truth here too */
    absorbDeviceEvent(it);         /* kind=airtag is the only source of truth here too */
    anti_.absorbEvent(it, now_); /* kind=airtag + deck-local movement correlation */

}

void DeckApp::requestScan(uint32_t now_ms)
{
    if(wardriveRowsPending() || wardrive_closing_)return;
    if (client_.state() != ocp::LinkState::Ready) return;
    if (!client_.send(OCP_V_SCAN_NETWORKS, now_ms)) {
        retrySoon([this](uint32_t t) { requestScan(t); }, now_ms);
        return;
    }
    wardrive_logged_upto_ = 0;
    wardrive_rescan_=false;
    scan_wait_started_ms_=now_ms;
    scan_.begin();
    scan_pending_ = true;
    next_page_ = 0;
    scan_started_ms_ = now_ms;
}

bool DeckApp::phyEngineActive() const
{
    return scan_.scanning() || scan_.continuousActive() || contacts_.sniffing() ||
           spectrum_.active() || deauth_.active() || bt_.scanning() || bt_.continuousActive() ||
           bt_.airtagActive() || anti_.active() || zig_.active();
}

void DeckApp::clearPhyBelief()
{
    anti_start_pending_ = false;
    bt_scan_pending_ = false;
    bt_continuous_pending_ = false;
    bt_airtag_pending_ = false;
    spectrum_start_pending_ = false;
    scan_.stop();
    contacts_.stopSniffing();
    spectrum_.stop();
    deauth_.stop();
    bt_.stop();
    anti_.stop();
    zig_.stop();
}

void DeckApp::clearLoraBelief()
{
    lora_.stop();
    storage::loraLogHealth(lora_.health());
    storage::loraLogEnd();
}

bool DeckApp::phyToolActive() const
{
    return phyEngineActive() || scan_pending_ || wifi_continuous_pending_ ||
           spectrum_start_pending_ || bt_scan_pending_ || bt_continuous_pending_ ||
           bt_airtag_pending_ || anti_start_pending_ || zig_start_pending_;
}

bool DeckApp::preparePhyStart(PhyHandoff::Start start, uint32_t now_ms)
{
    const auto request = phy_handoff_.request(phyToolActive(), client_.stopPending(), std::move(start));
    if (request == PhyHandoff::Request::StartNow) return true;
    if (request == PhyHandoff::Request::SendStop && !client_.stop(now_ms, OCP_LANE_PHY)) {
        phy_handoff_.clear();
        notice("unable to stop current PHY tool");
        return false;
    }
    notice("stopping current PHY tool...");
    return false;
}

void DeckApp::startScan(uint32_t now_ms)
{
    if (scan_.continuousActive()) { notice("stop live scan first"); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if(cursor_<scan_.rows().size())ap_focus_bssid_=scan_.rows()[cursor_].bssid;
    cursor_ = 0;
    screen_ = Screen::Sweep;
    if (!preparePhyStart([this](uint32_t now) { startScan(now); }, now_ms)) return;
    requestScan(now_ms);
    notice("");
}

void DeckApp::toggleWifiContinuous(uint32_t now_ms)
{
    if (scan_.continuousActive()) { stopPhy(now_ms); return; }
    if (wifi_continuous_pending_) { notice("starting live scan..."); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if(cursor_<scan_.rows().size())ap_focus_bssid_=scan_.rows()[cursor_].bssid;
    cursor_ = 0;
    screen_ = Screen::Sweep;
    if (!preparePhyStart([this](uint32_t now) { toggleWifiContinuous(now); }, now_ms)) return;
    if (!client_.send(OCP_V_START_WIFI_SCAN, now_ms)) {
        retrySoon([this](uint32_t t) { toggleWifiContinuous(t); }, now_ms);
        return;
    }
    wifi_continuous_pending_ = true;
    scan_.beginContinuous();
    scan_started_ms_ = now_ms;
    notice("");
}


void DeckApp::startSniffer(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    screen_ = Screen::Contacts;
    if (!preparePhyStart([this](uint32_t now) { startSniffer(now); }, now_ms)) return;
    if (!client_.send(OCP_V_START_SNIFFER, now_ms)) {
        retrySoon([this](uint32_t t) { startSniffer(t); }, now_ms);
        return;
    }
    contacts_.begin();
    contact_inspection_.refresh(contacts_);
    contacts_cursor_ = 0;
    contacts_tab_ = contact_inspection_.active() && contact_inspection_.key().probes ? ui::ContactsTab::Probes : ui::ContactsTab::Clients;
    contacts_poll_clients_ = true;
    last_contacts_poll_ms_ = now_ms;
    notice("");
}

void DeckApp::startChannelView(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (spectrum_start_pending_) { notice("starting spectrum..."); return; }
    if (!preparePhyStart([this](uint32_t now) { startChannelView(now); }, now_ms)) return;
    if (!client_.send(OCP_V_CHANNEL_VIEW, now_ms)) {
        retrySoon([this](uint32_t t) { startChannelView(t); }, now_ms);
        return;
    }
    spectrum_start_pending_ = true;
    spectrum_started_ms_ = now_ms;
    spectrum_cursor_ = 0;
    notice("starting spectrum...");
}

void DeckApp::startPacketMonitor(uint32_t now_ms, uint8_t ch)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!preparePhyStart([this, ch](uint32_t now) { startPacketMonitor(now, ch); }, now_ms)) return;
    if (!client_.send(std::string(OCP_V_PACKET_MONITOR) + " " + std::to_string(ch), now_ms)) {
        retrySoon([this, ch](uint32_t t) { startPacketMonitor(t, ch); }, now_ms);
        return;
    }
    scope_inspection_.save(spectrum_, spectrum_cursor_);
    spectrum_.beginLocked(ch);
    notice("");
}

namespace {
}  // namespace

/* Shared by the profile-cycling 'c'/'x' path and the debug console's
 * lora_manual command — one send/absorb sequence so they can't drift apart
 * (same reasoning as channel_plans.h's resolvedChannelForProfile() in
 * LoRaTrace-RX: one function every caller goes through). */
void DeckApp::sendLoraConfig(uint32_t freq_hz, int sf, int bw_khz, int cr, uint8_t sync_word,
                             const char *profile_token, uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    std::string cmd = std::string(OCP_V_LORA_CONFIG) + " " + std::to_string(freq_hz) + " " +
                       std::to_string(sf) + " " + std::to_string(bw_khz) + " " + std::to_string(cr) +
                       " " + std::to_string(sync_word);
    if (!client_.send(cmd, now_ms)) {
        retrySoon([this, freq_hz, sf, bw_khz, cr, sync_word, profile_token](uint32_t t) {
            sendLoraConfig(freq_hz, sf, bw_khz, cr, sync_word, profile_token, t);
        }, now_ms);
        return;
    }
    lora_config_sent_ = {freq_hz, sf, bw_khz, cr, sync_word, profile_token};
    lora_config_pending_ = true;   /* committed to lora_ only on [CFG], see onReply() */
    notice("configuring lora...");
}

/* Cycles with 'x' (Screen::SubGhz), applies with 'c'. Named presets only —
 * arbitrary manual entry has no on-device numeric input yet (this keyboard
 * has none built) and goes through the debug console's lora_manual command
 * instead, which also calls sendLoraConfig() above. */
void DeckApp::startLoraConfig(uint32_t now_ms)
{
    const auto &p = model::kLoraProfiles[lora_profile_index_];
    sendLoraConfig(p.freq_hz, p.sf, p.bw_khz, p.cr, p.sync_word, p.token, now_ms);
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
    if (!preparePhyStart([this](uint32_t now) { startDeauthDetector(now); }, now_ms)) return;
    if (!client_.send(OCP_V_DEAUTH_DETECTOR, now_ms)) {
        retrySoon([this](uint32_t t) { startDeauthDetector(t); }, now_ms);
        return;
    }
    deauth_.begin();
    deauth_cursor_ = 0;
    notice("");
}

bool DeckApp::bleBusyElsewhere(BleMode mine) const
{
    const bool scan_busy = bt_.scanning() || bt_scan_pending_;
    const bool continuous_busy = bt_.continuousActive() || bt_continuous_pending_;
    const bool airtag_busy = bt_.airtagActive() || bt_airtag_pending_;
    switch (mine) {
    case BleMode::Scan:       return continuous_busy || airtag_busy;
    case BleMode::Continuous: return scan_busy || airtag_busy;
    case BleMode::Airtag:     return scan_busy || continuous_busy;
    }
    return false;
}

void DeckApp::startBtScan(uint32_t now_ms)
{
    if (bleBusyElsewhere(BleMode::Scan)) {
        notice("stop current beacon mode first");
        return;
    }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!preparePhyStart([this](uint32_t now) { startBtScan(now); }, now_ms)) return;
    if (!client_.send(OCP_V_SCAN_BT, now_ms)) {
        retrySoon([this](uint32_t t) { startBtScan(t); }, now_ms);
        return;
    }
    if(bt_cursor_<bt_.devices().size())bt_focus_mac_=bt_.devices()[bt_cursor_].mac;
    bt_.beginScan();
    bt_scan_pending_ = true;
    bt_scan_started_ms_ = now_ms;
    bt_cursor_ = 0;
    notice("");
}

void DeckApp::toggleBtContinuous(uint32_t now_ms)
{
    if (bt_.continuousActive()) { stopPhy(now_ms); return; }
    if (bleBusyElsewhere(BleMode::Continuous)) {
        notice("stop current beacon mode first");
        return;
    }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!preparePhyStart([this](uint32_t now) { toggleBtContinuous(now); }, now_ms)) return;
    if (!client_.send(OCP_V_START_BLE_SCAN, now_ms)) {
        retrySoon([this](uint32_t t) { toggleBtContinuous(t); }, now_ms);
        return;
    }
    bt_continuous_pending_ = true;
    bt_cursor_ = 0;
    notice("starting beacons...");
}

void DeckApp::toggleAirtagScan(uint32_t now_ms)
{
    if (bt_.airtagActive()) { stopPhy(now_ms); return; }
    if (bleBusyElsewhere(BleMode::Airtag)) {
        notice("stop current beacon mode first");
        return;
    }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!preparePhyStart([this](uint32_t now) { toggleAirtagScan(now); }, now_ms)) return;
    if (!client_.send(OCP_V_SCAN_AIRTAG, now_ms)) {
        retrySoon([this](uint32_t t) { toggleAirtagScan(t); }, now_ms);
        return;
    }
    bt_airtag_pending_ = true;
    notice("starting tracker watch...");
}

void DeckApp::toggleAntisurveillance(uint32_t now_ms)
{
    if (anti_.active()) { stopPhy(now_ms); return; }
    if (anti_start_pending_) { notice("starting anti-surveillance..."); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!preparePhyStart([this](uint32_t now) { toggleAntisurveillance(now); }, now_ms)) return;
    if (!client_.send(OCP_V_START_ANTISURV, now_ms)) {
        retrySoon([this](uint32_t t) { toggleAntisurveillance(t); }, now_ms);
        return;
    }
    anti_start_pending_ = true;
    anti_cursor_ = 0;
    bt_.resetTrackerLog();
    notice("starting anti-surveillance...");
}

void DeckApp::toggleZig(uint32_t now_ms)
{
    if (zig_.active()) { stopPhy(now_ms); return; }
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!preparePhyStart([this](uint32_t now) { toggleZig(now); }, now_ms)) return;
    if (!client_.send(OCP_V_START_ZIG_RECON, now_ms)) { retrySoon([this](uint32_t t) { toggleZig(t); }, now_ms); return; }
    zig_start_pending_ = true;
    zig_.begin(); zig_cursor_ = 0; zig_tab_ = ui::ZigTab::Nodes; screen_ = Screen::Zig; notice("");
}

void DeckApp::feedGnss(const uint8_t *data, size_t len, uint32_t now_ms)
{
    now_ = now_ms;
    gnss_parser_.feed(data, len, [&](const gnss::Sentence &s) { gnss_.absorb(s, now_ms); });

    if (gnss_.state(now_ms)==model::GnssState::Fixed && gnss_.fixAgeMs(now_ms)<3000 &&
        now_ms - last_track_point_ms_ >= kTrackPointMs) {
        last_track_point_ms_ = now_ms;
        storage::wardriveLogTrackPoint(gnss_.fix());
    }
}

void DeckApp::toggleWardriveLog(uint32_t now_ms)
{
    if (storage::wardriveLogStats().open) {
        wardrive_closing_=true;wardrive_rescan_=false;
        notice("finishing queued rows...");return;
    }
    wardrive_closing_=false;wardrive_rescan_=false;

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
    if (!scan_.scanning() && preparePhyStart([this](uint32_t now) { requestScan(now); }, now_ms)) {
        requestScan(now_ms);
    }
}


void DeckApp::back(uint32_t now_ms)
{
    if(isSystemScreen(screen_)&&system_from_sessions_){screen_=Screen::Logs;system_from_sessions_=false;dirty_=true;return;}
    if(screen_==Screen::Logs){
        storage::sessionCancel(session_review_);
        if(session_review_.stage!=model::ReviewStage::Catalog){session_review_.back();dirty_=true;return;}
        screen_=system_navigation_.active()?system_navigation_.origin():Screen::Link;system_navigation_.clear();dirty_=true;return;
    }
    if(isSystemScreen(screen_) && system_navigation_.active()) {
        screen_=system_navigation_.origin();system_navigation_.clear();dirty_=true;return;
    }
    if(screen_==Screen::Contacts && contact_inspection_.active()){
        contacts_tab_=contact_inspection_.key().probes?ui::ContactsTab::Probes:ui::ContactsTab::Clients;
        contacts_cursor_=model::contactCursor(contacts_,contact_inspection_.key(),contacts_cursor_);
        contact_inspection_.close();dirty_=true;return;
    }
    if(screen_==Screen::Trace && wifi_inspection_.active()){
        const auto i=model::apIndex(scan_,wifi_inspection_.row().bssid);
        cursor_=i<scan_.rows().size()?i:(scan_.rows().empty()?0:std::min(cursor_,scan_.rows().size()-1));
        if(traceListening())stopPhy(now_ms);
        wifi_inspection_.close();screen_=Screen::Sweep;dirty_=true;return;
    }
    if(screen_==Screen::Beacons && bt_inspection_.active()){
        const auto i=model::btIndex(bt_,bt_inspection_.row().mac);
        bt_cursor_=i<bt_.devices().size()?i:(bt_.devices().empty()?0:std::min(bt_cursor_,bt_.devices().size()-1));
        bt_inspection_.close();dirty_=true;return;
    }
    if (screen_ == Screen::SubGhz && packet_inspection_.active()) {
        packet_inspection_.close();dirty_=true;return;
    }
    if (screen_ == Screen::Spectrum && spectrum_.locked()) {
        auto restore=[this](uint32_t) {
            if(!scope_inspection_.restore(spectrum_,spectrum_cursor_)) {
                spectrum_.begin();spectrum_.stop();spectrum_cursor_=0;
            }
            dirty_=true;
        };
        if(preparePhyStart(restore,now_ms))restore(now_ms);
        return;
    }
    /* A live sniffer/spectrum mode has no pending command once its ack
     * lands (it's a stream, not a blocking reply), so leaving the screen
     * must send `stop` unconditionally rather than only when something is
     * pending. Scoped per D-16: LoRa is its own sub-GHz lane and the rest
     * are the PHY lane — a bare stop here would cancel
     * whichever lane isn't actually being left, same bug D-16 fixed at the
     * protocol layer, just reachable again if this call didn't scope it
     * too. */
    if (screen_ == Screen::SubGhz) {
        client_.stop(now_ms, OCP_LANE_LORA);
    } else if (screen_ == Screen::Sweep || screen_ == Screen::Contacts || screen_ == Screen::Spectrum || screen_ == Screen::Zig || screen_ == Screen::Deauth || screen_ == Screen::AntiSurveillance || screen_ == Screen::Beacons) {
        stopPhy(now_ms);
    } else if (screen_ == Screen::Trace && client_.pending()) {
        stopPhy(now_ms);
    }
    screen_ = screen_ == Screen::Trace ? Screen::Sweep : Screen::Link;
    dirty_ = true;
}

void DeckApp::stopPhy(uint32_t now_ms)
{
    /* D-18: an ESP32-C5 coexistence defect with no app-level fix means
     * stopping 802.15.4 always costs the probe a recovery reboot —
     * immediately, unless LoRa RX is holding it off (probe_restart.c), in
     * which case it fires once LoRa itself stops. Surfacing this here (the
     * one chokepoint every "stop the PHY tool" path funnels through) so
     * the brief link drop reads as expected, not a fault, regardless of
     * whether the 802.15.4 card was left via 's', navigating away, or
     * `stop phy`. */
    if (zig_.active()) {
        notice(lora_.active() ? "802.15.4 stopped - probe will restart once LoRa stops"
                               : "802.15.4 stopped - probe restarting...");
    }
    phy_handoff_.clear();
    client_.stop(now_ms, OCP_LANE_PHY);
}

void DeckApp::stopPhyForNavigation(uint32_t now_ms)
{
    const Screen owner=system_navigation_.active()?system_navigation_.origin():screen_;
    if(owner==Screen::Trace && (traceListening()||scan_.continuousActive()||scan_.scanning())){stopPhy(now_ms);return;}
    if (owner != Screen::Sweep && owner != Screen::Contacts && owner != Screen::Spectrum &&
        owner != Screen::Zig && owner != Screen::Deauth && owner != Screen::AntiSurveillance && owner != Screen::Beacons) return;
    stopPhy(now_ms);
}

void DeckApp::onKeys(const Keys &keys, uint32_t now_ms)
{
    now_ = now_ms;
    if (!keys.chars.empty() || keys.enter) log("keys=" + keys.chars + (keys.enter ? "<enter>" : ""));
    for (char c : keys.chars) {
        if (c == '`') {
            if (help_visible_) {
                help_visible_ = false;
                dirty_ = true;
            } else if(isSystemScreen(screen_) && system_navigation_.active()) {
                back(now_ms);
            } else if (screen_ == Screen::Settings) {
                screen_ = Screen::Link;
                dirty_ = true;
            } else if (screen_ == Screen::Link) {
                screen_ = Screen::Settings;
                dirty_ = true;
            } else {
                back(now_ms);
            }
            continue;
        }
        if (c == 'h') {
            help_visible_ = !help_visible_;
            dirty_ = true;
            continue;
        }
        if (help_visible_) continue;
        if(c=='o'){openSessions(now_ms);continue;}
        if(c=='i') {
            if(screen_==Screen::Logs)system_from_sessions_=true;
            if(isSystemScreen(screen_) && system_navigation_.active())back(now_ms);
            else navigateCard(Screen::Info,now_ms);
            continue;
        }
        if (c == 'd') { toggleDebugMode(); continue; }
        if ((c == ',' || c == '/') && isHomeCard(screen_)) {
            navigateCard(cycleScreen(screen_, c == '/' ? 1 : -1),now_ms);
            dirty_ = true;
            continue;
        }

        switch (screen_) {
        case Screen::Link:
            if (c == ';' && link_cursor_ > 0) link_cursor_--;
            else if (c == '.' && link_cursor_ + 1 < ui::kLinkRowCount) link_cursor_++;
            else if (c == 'w') startScan(now_ms);
            else if (c == 'p') client_.send(OCP_V_PING, now_ms);
            else if (c == 's') client_.send(OCP_V_STATUS, now_ms);
            else if (c == 'r') client_.send(OCP_V_REBOOT, now_ms);
            break;
        case Screen::Settings:
            if (c == ';') adjustBrightness(1);
            else if (c == '.') adjustBrightness(-1);
            else if (c == ',') changeWifiRegion(app::cycleRegion(wifi_region_, -1).code);
            else if (c == '/') changeWifiRegion(app::cycleRegion(wifi_region_, 1).code);
            break;
        case Screen::Sweep:
            if (c == ';' && cursor_ > 0) cursor_--;
            else if (c == '.' && cursor_ + 1 < scan_.rows().size()) cursor_++;
            else if (c == 'r') startScan(now_ms);
            else if (c == 's' || c == 'c') toggleWifiContinuous(now_ms);
            if(cursor_<scan_.rows().size())ap_focus_bssid_=scan_.rows()[cursor_].bssid;
            break;
        case Screen::Trace:
            if(c=='x')wifi_inspection_.togglePage();
            else if(c==';'&&wifi_inspection_.clientsPage())wifi_inspection_.movePeer(contacts_,-1);
            else if(c=='.'&&wifi_inspection_.clientsPage())wifi_inspection_.movePeer(contacts_,1);
            else if(c=='r')refreshWifiDossier(now_ms);
            break;
        case Screen::Contacts: {
            if(contact_inspection_.active()){
                if(c=='s'){
                    if(contacts_.sniffing())stopPhy(now_ms);
                    else startSniffer(now_ms);
                }
                break;
            }
            size_t n = contacts_tab_ == ui::ContactsTab::Clients ? contacts_.clients().size()
                                                                  : contacts_.probes().size();
            if (c == ';' && contacts_cursor_ > 0) contacts_cursor_--;
            else if (c == '.' && contacts_cursor_ + 1 < n) contacts_cursor_++;
            else if (c == 'x') {
                contacts_tab_ = contacts_tab_ == ui::ContactsTab::Clients ? ui::ContactsTab::Probes
                                                                          : ui::ContactsTab::Clients;
                contacts_cursor_ = 0;
            } else if (c == 's') {
                if (contacts_.sniffing()) stopPhy(now_ms);
                else startSniffer(now_ms);
            }
            break;
        }
        case Screen::Logs:
            sessionKeys(c);break;
        case Screen::Info:
            break;   /* nothing but card-cycling and back here */
        case Screen::Spectrum:
            if (c == ';' && spectrum_cursor_ > 0) spectrum_cursor_--;
            else if (c == '.' && spectrum_cursor_ + 1 < spectrum_.readings().size()) spectrum_cursor_++;
            else if (c == 's') {
                if (spectrum_.active()) stopPhy(now_ms);
                else if(spectrum_.locked()) startPacketMonitor(now_ms,spectrum_.lockedChannel());
                else startChannelView(now_ms);
            }
            break;
        case Screen::Zig: {
            size_t n = zig_tab_ == ui::ZigTab::Pans ? zig_.pans().size() : zig_.nodes().size();
            if (c == ';' && zig_cursor_ > 0) zig_cursor_--;
            else if (c == '.' && zig_cursor_ + 1 < n) zig_cursor_++;
            else if (c == 'x') {
                zig_tab_ = zig_tab_ == ui::ZigTab::Pans ? ui::ZigTab::Nodes : ui::ZigTab::Pans;
                zig_cursor_ = 0;
            } else if (c == 's') toggleZig(now_ms);
            break;
        }
        case Screen::SubGhz:
            if(packet_inspection_.active()) {
                if(c==';')packet_inspection_.page(-1);
                else if(c=='.')packet_inspection_.page(1);
                else if(c=='s') {
                    if(lora_.active())client_.stop(now_ms,OCP_LANE_LORA);
                    else startLoraListen(now_ms);
                }
                break;
            }
            if (c == ';' && lora_cursor_ > 0) lora_cursor_--;
            else if (c == '.' && lora_cursor_ + 1 < lora_.packets().size()) lora_cursor_++;
            else if (c == 'x') {
                lora_profile_index_ = (lora_profile_index_ + 1) % model::kLoraProfileCount;
            }
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
                if (deauth_.active()) stopPhy(now_ms);
                else startDeauthDetector(now_ms);
            }
            break;
        case Screen::AntiSurveillance:
            if (c == ';' && anti_cursor_ > 0) anti_cursor_--;
            else if (c == '.' && anti_cursor_ + 1 < anti_.trackers().size()) anti_cursor_++;
            else if (c == 's') toggleAntisurveillance(now_ms);
            break;
        case Screen::Drive:
            if (c == 'l') toggleWardriveLog(now_ms);
            break;
        case Screen::Beacons:
            if(bt_inspection_.active()){
                if(c=='r')refreshBtDossier(now_ms);
                else if(c=='s'){if(bt_.scanning()||bt_.continuousActive()||bt_.airtagActive())stopPhy(now_ms);else refreshBtDossier(now_ms);}
                break;
            }
            if (c == ';' && bt_cursor_ > 0) bt_cursor_--;
            else if (c == '.' && bt_cursor_ + 1 < bt_.devices().size()) bt_cursor_++;
            else if (c == 's') startBtScan(now_ms);
            else if (c == 'c') toggleBtContinuous(now_ms);
            else if (c == 'a') toggleAirtagScan(now_ms);
            break;
        }
        dirty_ = true;
    }
    if(keys.enter&&!help_visible_&&screen_==Screen::Logs)sessionEnter();
    if (keys.enter && !help_visible_ && screen_ == Screen::Link) {
        switch (link_cursor_) {
        case 0: /* Probe Link: reconnect when needed, otherwise verify the link. */
            if (client_.state() == ocp::LinkState::Disconnected) client_.connect(now_ms);
            else client_.send(OCP_V_PING, now_ms);
            break;
        case 1: /* GNSS: the Drive card owns the position/session workflow. */
            screen_ = Screen::Drive;
            dirty_ = true;
            break;
        case 2: /* SD Card: system health currently owns storage readiness. */
            screen_ = Screen::Info;
            dirty_ = true;
            break;
        case 3: /* Bus Integrity: a fresh ping updates the transport result. */
            client_.send(OCP_V_PING, now_ms);
            break;
        }
    }
    if(keys.enter && !help_visible_ && screen_==Screen::Contacts && !contact_inspection_.active())openContactInspection();
    if (keys.enter && !help_visible_ && screen_ == Screen::SubGhz && !packet_inspection_.active() &&
        lora_cursor_ < lora_.packets().size()) {
        packet_inspection_.open(lora_.packets()[lora_cursor_]);dirty_=true;
    }
    if(keys.enter&&!help_visible_&&screen_==Screen::Sweep)openWifiDossier();
    if(keys.enter&&!help_visible_&&screen_==Screen::Beacons&&!bt_inspection_.active())openBtDossier();
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

void DeckApp::adjustBrightness(int direction)
{
    constexpr int kStep = 24;
    /* Never below this: a screen dim enough to be unreadable strands the
     * user with no way back into Settings to raise it again. */
    constexpr int kBrightnessMin = 24;
    constexpr int kBrightnessMax = 255;
    int value = static_cast<int>(brightness_) + direction * kStep;
    if (value < kBrightnessMin) value = kBrightnessMin;
    if (value > kBrightnessMax) value = kBrightnessMax;
    brightness_ = static_cast<uint8_t>(value);
    M5Cardputer.Display.setBrightness(brightness_);
    storage::saveBrightness(brightness_);
    dirty_ = true;
}

void DeckApp::changeWifiRegion(const std::string &code)
{
    const ocp_wifi_region_t *r = ocp_wifi_region_find(code.c_str());
    if (!r) {
        notice("unknown region " + code);
        return;
    }
    wifi_region_ = r->code;
    storage::saveWifiRegion(wifi_region_);
    region_push_attempted_ = false;
    region_notice_pending_ = true;
    log("wifi region set " + wifi_region_);
    if (client_.state() == ocp::LinkState::Ready && !probe_region_.empty() &&
        probe_region_ != wifi_region_ && phyToolActive()) {
        notice(std::string("region ") + r->code + " - applies when radio idle");
    }
    dirty_ = true;
}

void DeckApp::pushWifiRegionIfDue(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready || client_.pending() || client_.stopPending() ||
        next_page_ || pending_retry_ || phy_handoff_.queued()) return;
    const bool idle = !phyToolActive() && (probe_owner_.empty() || probe_owner_ == "none");
    if (!app::regionPushDue(wifi_region_, probe_region_, idle, region_push_attempted_,
                            now_ms - region_push_ms_)) return;
    if (client_.send(std::string(OCP_V_SET_REGION) + " " + wifi_region_, now_ms)) {
        region_push_pending_ = true;
        region_push_attempted_ = true;
        region_push_ms_ = now_ms;
        log("set_region " + wifi_region_);
    }
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
    else if (cmd == "region") changeWifiRegion(arg);
    else if (cmd == "stop") {
        if (arg.empty() || arg == OCP_LANE_PHY) phy_handoff_.clear();
        client_.stop(now_ms, arg.empty() ? OCP_LANE_ALL : arg);
    }
    else if (cmd == "sniff") { if (contacts_.sniffing()) stopPhy(now_ms); else startSniffer(now_ms); }
    else if (cmd == "spectrum") { if (spectrum_.active()) stopPhy(now_ms); else startChannelView(now_ms); }
    else if (cmd == "deauth") { if (deauth_.active()) stopPhy(now_ms); else startDeauthDetector(now_ms); }
    else if (cmd == "wifiscan") toggleWifiContinuous(now_ms);
    else if (cmd == "beacons") startBtScan(now_ms);
    else if (cmd == "blescan") toggleBtContinuous(now_ms);
    else if (cmd == "airtag") toggleAirtagScan(now_ms);
    else if (cmd == "antisurv") toggleAntisurveillance(now_ms);
    else if (cmd == "zig") toggleZig(now_ms);
    else if (cmd == "lora") {
        if (arg == "config") startLoraConfig(now_ms);
        else if (lora_.active()) client_.stop(now_ms, OCP_LANE_LORA);
        else startLoraListen(now_ms);
    }
    else if (cmd == "lora_manual") {
        /* lora_manual <freq_hz> <sf> <bw_khz> <cr> [sync_word] — the only
         * arbitrary-parameter path until this keyboard has real numeric
         * entry (see startLoraConfig's comment). Whitespace-split by hand,
         * same idiom as cmd/arg's own split above. */
        std::string rest = arg;
        std::string tok[5];
        size_t n = 0;
        while (n < 5 && !rest.empty()) {
            size_t sp = rest.find(' ');
            tok[n++] = rest.substr(0, sp);
            rest = sp == std::string::npos ? "" : rest.substr(sp + 1);
        }
        if (n < 4) { log("debug: lora_manual needs freq_hz sf bw_khz cr [sync_word]"); return; }
        uint64_t freq = 0, sf = 0, bw = 0, cr = 0, sync = 0x12;
        bool ok = model::parseUnsigned(tok[0], &freq) && model::parseUnsigned(tok[1], &sf) &&
                  model::parseUnsigned(tok[2], &bw) && model::parseUnsigned(tok[3], &cr);
        if (ok && n == 5) ok = model::parseUnsigned(tok[4], &sync) && sync <= 255;
        /* Bound before the narrowing casts below; the probe still owns the
         * real SX1262 range checks and answers badarg on anything else. */
        if (ok) ok = freq <= UINT32_MAX && sf <= 255 && bw <= 65535 && cr <= 255;
        if (!ok) { log("debug: lora_manual args must be whole unsigned integers, sync_word 0..255"); return; }
        sendLoraConfig(static_cast<uint32_t>(freq), static_cast<int>(sf), static_cast<int>(bw),
                       static_cast<int>(cr), static_cast<uint8_t>(sync), "manual", now_ms);
    }
    else if (cmd == "channel") {
        if (arg.empty()) { log("debug: channel needs a number"); return; }
        uint64_t ch = 0;
        if (!model::parseUnsigned(arg, &ch) || ch > 255) { log("debug: channel is invalid"); return; }
        startPacketMonitor(now_ms, static_cast<uint8_t>(ch));
    }
    else if (cmd == "inspect") {
        wifi_inspection_.close();
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
        navigateCard(s,now_ms);
        dirty_ = true;
    }
    else if(cmd=="gnss_test")gnssDiagnostic(arg,now_ms);
    else if(cmd=="key") {
        Keys keys;
        if(arg=="enter")keys.enter=true;
        else if(arg=="back")keys.chars="`";
        else if(arg.size()==1 && static_cast<unsigned char>(arg[0])>=32 && static_cast<unsigned char>(arg[0])<=126)keys.chars=arg;
        else {log("debug: key needs one character, enter or back");return;}
        onKeys(keys,now_ms);
    }
    else if (cmd == "dump") {log(debugSnapshot(now_ms));return;}
    else { log("debug: unknown cmd=" + cmd); return; }

    log("debug: ok " + line);
}

void DeckApp::tick(uint32_t now_ms)
{
    drive_trail_.observe(gnss_.fix(),gnss_.state(now_ms)==model::GnssState::Fixed && gnss_.fixAgeMs(now_ms)<3000,
                         now_ms-gnss_.fixAgeMs(now_ms),now_ms,storage::wardriveLogStats().open);

    gnss_noise_.observe(gnss_.fix(),gnss_.state(now_ms)==model::GnssState::Fixed&&gnss_.fixAgeMs(now_ms)<3000,
                        now_ms-gnss_.fixAgeMs(now_ms));
    now_ = now_ms;

    if (!notice_.empty() && now_ms - notice_started_ms_ >= kNoticeDurationMs) {
        notice_.clear();
        notice_started_ms_ = 0;
        dirty_ = true;
    }

    client_.tick(now_ms);
    tickWardrive(now_ms);
    tickSessions(now_ms);

    /* Client timeouts intentionally have no callback. Clean up the optimistic
     * UI state here so a lost/invalid CFG cannot leave anti-surveillance
     * visibly stuck in "starting". */
    if (anti_start_pending_ && !client_.pending()) {
        anti_start_pending_ = false;
        anti_.stop();
        notice("anti-surveillance start timed out");
        log("anti start timeout");
    }

    /* A reply clears the client's pending state and runs onReply() in the
     * same step, so a flag still set once the client is idle means the reply
     * never came. Left set, the next unrelated [CFG] would commit a stale
     * config into lora_ and the next session manifest. */
    if (lora_config_pending_ && !client_.pending()) {
        lora_config_pending_ = false;
        notice("lora config timed out");
        log("lora config timeout");
    }

    if (spectrum_start_pending_ && !client_.pending() &&
        now_ms - spectrum_started_ms_ >= ocp::Client::kReplyTimeoutMs) {
        spectrum_start_pending_ = false;
        spectrum_.clear();
        notice("spectrum start timed out");
        log("spectrum start timeout");
    }

    if (scan_pending_ && scan_.scanning() && !client_.pending() && !next_page_ && !wardriveRowsPending() &&
        now_ms - scan_wait_started_ms_ >= ocp::Client::kScanTimeoutMs) {
        scan_pending_ = false;
        scan_.clear();
        next_page_ = 0;
        notice("sweep timed out");
        log("sweep timeout");
    }

    if (!client_.pending()) {
        if (bt_scan_pending_ || bt_continuous_pending_ || bt_airtag_pending_) {
            bt_scan_pending_ = false;
            bt_continuous_pending_ = false;
            bt_airtag_pending_ = false;
            bt_.stop();
            notice("beacon start timed out");
            log("beacon start timeout");
        }
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

    if (next_page_ && !client_.pending() && !wardriveRowsPending() && !wardrive_closing_) {
        uint16_t first = next_page_;
        next_page_ = 0;
        if (!client_.send(std::string(OCP_V_SHOW_SCAN_RESULTS) + " " + std::to_string(first), now_ms)) {
            next_page_ = first;   /* retry next tick */
        } else scan_wait_started_ms_=now_ms;
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
    if (screen_ == Screen::Zig && zig_.active() && st == ocp::LinkState::Ready && !client_.pending() &&
        now_ms - last_zig_poll_ms_ >= kZigPollMs) {
        last_zig_poll_ms_ = now_ms;
        client_.send(OCP_V_ZIG_LIST, now_ms);
    }
    if (lora_.active() && st == ocp::LinkState::Ready && !client_.pending() &&
        now_ms - last_lora_health_poll_ms_ >= kLoraHealthPollMs) {
        last_lora_health_poll_ms_ = now_ms;
        client_.send(OCP_V_LORA_STATUS, now_ms);
    }
    pushWifiRegionIfDue(now_ms);
    if (st == ocp::LinkState::Ready && !client_.pending() && !next_page_ &&
        !pending_retry_ && !phy_handoff_.queued() && !scan_.scanning() && !bt_.scanning() &&
        now_ms-last_status_poll_ms_>=kInfoPollMs) {
        if(client_.send(OCP_V_STATUS,now_ms))last_status_poll_ms_=now_ms;
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
    bool busy = (screen_==Screen::Logs&&session_review_.busy) || scan_.scanning() || scan_.continuousActive() || bt_.scanning() ||
                (screen_ == Screen::Trace && client_.pending());
    return dirty_ || (busy && now_ms - last_draw_ms_ >= kBusyRedrawMs);
}

void DeckApp::draw(uint32_t now_ms)
{
    auto makeChrome = [&](const char *title) {
        ui::ChromeState chrome;
        chrome.title = title;
        chrome.link = client_.state();
        switch (gnss_.state(now_ms)) {
        case model::GnssState::Fixed:
            chrome.gnss = ui::IndicatorState::Ready;
            break;
        case model::GnssState::NoUartData:
            chrome.gnss = ui::IndicatorState::Fault;
            break;
        case model::GnssState::Searching:
        case model::GnssState::FixLost:
            chrome.gnss = ui::IndicatorState::Pending;
            break;
        }
        chrome.storage = storage::ready() ? ui::IndicatorState::Ready : ui::IndicatorState::Fault;
        const bool phy_active = phyEngineActive();
        chrome.rf = (phy_active || lora_.active()) ? ui::IndicatorState::Active : ui::IndicatorState::Off;
        if (!battery_polled_ || now_ms - last_battery_poll_ms_ >= kBatteryPollMs) {
            battery_polled_ = true;
            last_battery_poll_ms_ = now_ms;
            cached_battery_pct_ = M5.Power.getBatteryLevel();
            cached_charging_ = M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging;
        }
        chrome.battery_pct = cached_battery_pct_;
        chrome.charging = cached_charging_;
        chrome.live = phy_active || lora_.active();
        chrome.debug = debug_mode_;
        char transport[64];
        int len = std::snprintf(transport, sizeof transport, "BPK %s", ocp::linkStateName(client_.state()));
        if (last_reply_ != "-" && len > 0 && static_cast<size_t>(len) < sizeof transport) {
            len += std::snprintf(transport + len, sizeof(transport) - len, " <-> %s", last_reply_.c_str());
        }
        if (chrome.live && len > 0 && static_cast<size_t>(len) < sizeof transport) {
            std::snprintf(transport + len, sizeof(transport) - len, " · RX-ONLY");
        }
        chrome.transport = transport;
        chrome.notice = notice_;
        return chrome;
    };

    if (help_visible_) {
        ui::drawHelpView(makeChrome("HELP"));
    } else switch (screen_) {
    case Screen::Link:
        ui::drawLinkView(client_, link_cursor_, makeChrome("LINK"));
        break;
    case Screen::Settings:
    {
        const auto &region = app::regionOrDefault(wifi_region_);
        char channels[64];
        ocp_wifi_region_summary(&region, channels, sizeof channels);
        const auto state = app::regionState(client_.state() == ocp::LinkState::Ready,
                                            wifi_region_, probe_region_);
        ui::drawSettingsView(brightness_, region.code, channels, app::regionStateLabel(state),
                             state == app::RegionState::Applied, makeChrome("SETTINGS"));
    }
        break;
    case Screen::Sweep:
        ui::drawSweepView(scan_, cursor_, now_ms - scan_started_ms_,
                          makeChrome("WIFI SCAN"));
        break;
    case Screen::Trace:
        ui::drawWifiDetailView(wifi_inspection_,scan_,contacts_,traceListening(),now_ms,makeChrome("AP DOSSIER"));
        break;
    case Screen::Contacts:
        if(contact_inspection_.active())ui::drawContactDetailView(contact_inspection_,contacts_.sniffing(),now_ms,makeChrome(contact_inspection_.key().probes?"REQUEST DETAIL":"CLIENT DETAIL"));
        else ui::drawContactsView(contacts_, contacts_tab_, contacts_cursor_,
                             makeChrome("SNIFFER"));
        break;
    case Screen::Info:
        ui::drawInfoView(client_, probe_status_valid_, probe_heap_, probe_heap_total_, probe_uptime_ms_,
                         makeChrome("SYSTEM"));
        break;
    case Screen::Spectrum:
        ui::drawSpectrumView(spectrum_, spectrum_cursor_,
                             makeChrome("PACKET MONITOR"));
        break;
    case Screen::Zig:
        ui::drawZigView(zig_, zig_tab_, zig_cursor_,
                         makeChrome("802.15.4"));
        break;
    case Screen::SubGhz:
        if(packet_inspection_.active())ui::drawLoraDetailView(packet_inspection_,makeChrome("PACKET HEX"));
        else ui::drawSubGhzView(lora_, lora_cursor_, makeChrome("LORA RX"),
                           model::kLoraProfiles[lora_profile_index_].label);
        break;
    case Screen::Deauth:
        ui::drawDeauthView(deauth_, deauth_cursor_,
                         makeChrome("DEAUTH DETECT"));
        break;
    case Screen::AntiSurveillance:
        ui::drawAntiSurveillanceView(anti_, anti_cursor_, anti_start_pending_,
                                     makeChrome("ANTI-SURV"));
        break;
    case Screen::Drive:
        ui::drawGnssView(gnss_, now_ms,
                         makeChrome("DRIVE / GNSS"),wardrive_closing_);
        break;
    case Screen::Logs:
        ui::drawSessionView(session_review_,makeChrome("SESSIONS"));break;
    case Screen::Beacons:
        if(bt_inspection_.active())ui::drawBtDetailView(bt_inspection_,bt_,now_ms,makeChrome("BLE DOSSIER"));
        else ui::drawBtView(bt_, bt_cursor_,
                       now_ms - bt_scan_started_ms_,
                       makeChrome("BLE SCAN"));
        break;
    }
    ui::present();
    dirty_ = false;
    last_draw_ms_ = now_ms;
}

}  // namespace app
