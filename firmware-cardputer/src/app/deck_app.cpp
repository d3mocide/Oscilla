/*
 * deck_app.cpp — see deck_app.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/deck_app.h"

#include <cstdlib>

#include "ocp.h"
#include "ui/canvas.h"
#include "ui/contacts_view.h"
#include "ui/deauth_view.h"
#include "storage/lora_logger.h"
#include "ui/info_view.h"
#include "ui/link_view.h"
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
constexpr uint32_t kInfoPollMs = 2000;       /* how often to refresh the probe's heap/uptime */
constexpr uint32_t kPendingRetryWindowMs = 4000;   /* generous vs. any single command's own reply timeout */

/* radio_arbiter's own message text (wifi_sniff.c/wifi_spectrum.c/
 * wifi_recon.c/wifi_inspect.c/wifi_deauth.c all format it identically) -
 * the one thing distinguishing a real cross-engine PHY conflict from
 * OCP_ERR_BUSY's other uses (scan_networks' own "scan in progress",
 * lora_listen's "already listening"), neither of which the arbiter is
 * involved in at all. Coupled to that exact wording on purpose: only the
 * arbiter conflict should trigger an automatic stop-and-switch. */
constexpr char kArbiterBusyPrefix[] = "radio in use by ";

/* The home cards: cycled with `,` (left/prev) and `/` (right/next), the
 * physical arrow-key cluster on the Cardputer's keyboard. Sweep/Trace are a
 * drill-down from Link instead (DESIGN §7.3), not part of this cycle. */
constexpr Screen kCards[] = { Screen::Link,   Screen::Contacts, Screen::Info,
                              Screen::Spectrum, Screen::SubGhz, Screen::Deauth };
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
            /* Same reasoning: whatever these were tracking is moot once the
             * link isn't Ready, and a stale one must never fire later
             * against something unrelated after a reconnect. */
            pending_retry_ = nullptr;
            armed_busy_retry_ = nullptr;
            handoff_retry_ = nullptr;
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
}

void DeckApp::onReply(const ocp::Item &it)
{
    dirty_ = true;
    /* Whatever armed_busy_retry_ was tracking is resolving right now, one
     * way or another - captured once here so every path below (including
     * the early Pong return) leaves it clean rather than needing to
     * remember to clear it in each branch. */
    auto armed_retry = std::move(armed_busy_retry_);
    armed_busy_retry_ = nullptr;

    if (it.kind == ocp::ItemKind::Pong) { last_reply_ = "pong"; return; }

    if (it.kind == ocp::ItemKind::Error) {
        lora_listen_pending_ = false;   /* rejected: no session, no file (see deck_app.h) */
        const auto *code = it.get(OCP_K_CODE);
        const auto *msg = it.get(OCP_K_MSG);
        bool arbiter_conflict = armed_retry && code && *code == OCP_ERR_BUSY && msg &&
                                 msg->rfind(kArbiterBusyPrefix, 0) == 0;
        if (arbiter_conflict) {
            /* A real same-PHY-lane conflict, not the pending_retry_ queuing
             * artifact: another Wi-Fi-family engine holds the radio. The
             * user's own explicit start action already signaled intent to
             * switch, so hand off immediately rather than just erroring -
             * stop the old one, then run the retry once its [STOP] lands. */
            handoff_retry_ = std::move(armed_retry);
            notice("switching radio...");
            client_.stop(now_);
        } else {
            notice(std::string("error ") + (code ? *code : "?") + ": " + (msg ? *msg : ""));
        }
        log(std::string("err code=") + (code ? *code : "?"));
        return;
    }

    last_reply_ = it.tag;
    if (it.tag == OCP_MARK_STATUS) {
        const auto *heap = it.get(OCP_K_HEAP);
        const auto *uptime = it.get(OCP_K_UPTIME_MS);
        if (heap && uptime) {
            probe_heap_ = static_cast<uint32_t>(std::strtoul(heap->c_str(), nullptr, 10));
            probe_uptime_ms_ = std::strtoull(uptime->c_str(), nullptr, 10);
            probe_status_valid_ = true;
            last_status_reply_ms_ = now_;
        }
    } else if (it.tag == OCP_MARK_STOP) {
        contacts_.stopSniffing();
        spectrum_.stop();
        lora_.stop();
        storage::loraLogEnd();
        deauth_.stop();
        if (handoff_retry_) {
            /* The engine that was in the way just released the PHY lane -
             * now actually run the start the user originally asked for. */
            auto retry = std::move(handoff_retry_);
            handoff_retry_ = nullptr;
            retry(now_);
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
        log("cfg ack ch=" + (ch ? *ch : std::string("?")));
    } else if (it.tag == OCP_MARK_SCAN) {
        next_page_ = scan_.absorbPage(it);   /* requested from tick(): not re-entrant here */
        const auto *first = it.get(OCP_K_FIRST);
        log("scan-page first=" + (first ? *first : std::string("?")) + " rows=" + std::to_string(it.rows.size()) +
            (next_page_ ? " next=" + std::to_string(next_page_) : std::string()));
        if (scan_.aborted()) {
            notice("scan stopped");
            log("scan aborted");
        } else if (!scan_.scanning()) {
            notice(scan_.truncated() ? "list capped at 512" : "");
            log("scan done aps=" + std::to_string(scan_.rows().size()) + " total=" + std::to_string(scan_.total()) +
                " malformed=" + std::to_string(scan_.malformedRows()) + " elapsed_ms=" + std::to_string(scan_.elapsedMs()));
        }
    } else if (it.tag == OCP_MARK_INSPECT) {
        scan_.absorbInspect(it);
        const auto &in = scan_.inspect();
        if (in.aborted) notice("inspect stopped");
        log("inspect idx=" + std::to_string(in.idx) + " beacons=" + std::to_string(in.beacons) +
            " rsn=" + std::to_string(in.rsn) + " mfp_capable=" + std::to_string(in.mfp_capable) +
            " mfp_required=" + std::to_string(in.mfp_required) + " aborted=" + std::to_string(in.aborted));
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
}

void DeckApp::startScan(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_SCAN_NETWORKS, now_ms)) {
        retrySoon([this](uint32_t t) { startScan(t); }, now_ms);
        return;
    }
    armed_busy_retry_ = [this](uint32_t t) { startScan(t); };
    scan_.begin();
    cursor_ = 0;
    next_page_ = 0;
    scan_started_ms_ = now_ms;
    screen_ = Screen::Sweep;
    notice("");
}

void DeckApp::startInspect(uint32_t now_ms)
{
    if (scan_.rows().empty()) return;
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    trace_idx_ = scan_.rows()[cursor_].idx;
    if (!client_.send(std::string(OCP_V_INSPECT_NETWORK) + " " + std::to_string(trace_idx_), now_ms)) {
        retrySoon([this](uint32_t t) { startInspect(t); }, now_ms);
        return;
    }
    armed_busy_retry_ = [this](uint32_t t) { startInspect(t); };
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
    armed_busy_retry_ = [this](uint32_t t) { startSniffer(t); };
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
    armed_busy_retry_ = [this](uint32_t t) { startChannelView(t); };
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
    armed_busy_retry_ = [this, ch](uint32_t t) { startPacketMonitor(t, ch); };
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
    /* No arbiter involved - LoRa is a separate chip, never PHY_OWNER_WIFI -
     * so no armed_busy_retry_ here; a busy reply for this verb can only be
     * the client-side queuing case retrySoon() already covers. */
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
    armed_busy_retry_ = [this](uint32_t t) { startDeauthDetector(t); };
    deauth_.begin();
    deauth_cursor_ = 0;
    notice("");
}

void DeckApp::back(uint32_t now_ms)
{
    /* A live sniffer/spectrum mode has no pending command once its ack
     * lands (it's a stream, not a blocking reply), so leaving the screen
     * must send `stop` unconditionally rather than only when something is
     * pending. */
    if (client_.pending() || screen_ == Screen::Contacts || screen_ == Screen::Spectrum ||
        screen_ == Screen::SubGhz || screen_ == Screen::Deauth) {
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
                if (contacts_.sniffing()) client_.stop(now_ms);
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
                if (spectrum_.active()) client_.stop(now_ms);
                else startChannelView(now_ms);
            }
            break;
        case Screen::SubGhz:
            if (c == ';' && lora_cursor_ > 0) lora_cursor_--;
            else if (c == '.' && lora_cursor_ + 1 < lora_.packets().size()) lora_cursor_++;
            else if (c == 'c') startLoraConfig(now_ms);
            else if (c == 's') {
                if (lora_.active()) client_.stop(now_ms);
                else startLoraListen(now_ms);
            }
            break;
        case Screen::Deauth:
            if (c == ';' && deauth_cursor_ > 0) deauth_cursor_--;
            else if (c == '.' && deauth_cursor_ + 1 < deauth_.events().size()) deauth_cursor_++;
            else if (c == 's') {
                if (deauth_.active()) client_.stop(now_ms);
                else startDeauthDetector(now_ms);
            }
            break;
        }
        dirty_ = true;
    }
    if (keys.enter && screen_ == Screen::Sweep && !scan_.scanning()) startInspect(now_ms);
    if (keys.enter && screen_ == Screen::Spectrum && !spectrum_.locked() &&
        spectrum_cursor_ < spectrum_.readings().size()) {
        startPacketMonitor(now_ms, spectrum_.readings()[spectrum_cursor_].ch);
    }
}

void DeckApp::tick(uint32_t now_ms)
{
    now_ = now_ms;
    client_.tick(now_ms);

    /* armed_busy_retry_ is only ever meaningful while the command it was
     * set for is still in flight. onReply() already clears it the moment
     * that command's reply lands, success or error - the one case that
     * misses is a silent timeout (Client::tick() clears pending() itself
     * without calling onReply()), which this backstop catches instead. */
    if (armed_busy_retry_ && !client_.pending()) armed_busy_retry_ = nullptr;

    if (pending_retry_) {
        if (!client_.pending()) {
            auto action = std::move(pending_retry_);
            pending_retry_ = nullptr;
            action(now_ms);   /* may itself queue another retry if still busy */
        } else if (now_ms - pending_retry_started_ms_ >= kPendingRetryWindowMs) {
            pending_retry_ = nullptr;
            notice("busy (gave up)");
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
    if (st == ocp::LinkState::Ready && !client_.pending() && !next_page_ &&
        now_ms - last_keepalive_ms_ >= kKeepaliveMs) {
        last_keepalive_ms_ = now_ms;
        client_.send(OCP_V_PING, now_ms);
    }
}

bool DeckApp::dirty(uint32_t now_ms) const
{
    if (now_ms - last_draw_ms_ < kRedrawMs) return false;
    bool busy = scan_.scanning() || (screen_ == Screen::Trace && client_.pending());
    return dirty_ || (busy && now_ms - last_draw_ms_ >= kBusyRedrawMs);
}

void DeckApp::draw(uint32_t now_ms)
{
    switch (screen_) {
    case Screen::Link:
        ui::drawLinkView(client_, last_reply_, notice_);
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
    case Screen::SubGhz:
        ui::drawSubGhzView(lora_, lora_cursor_, notice_);
        break;
    case Screen::Deauth:
        ui::drawDeauthView(deauth_, deauth_cursor_, notice_);
        break;
    }
    ui::present();
    dirty_ = false;
    last_draw_ms_ = now_ms;
}

}  // namespace app
