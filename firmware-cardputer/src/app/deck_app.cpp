/*
 * deck_app.cpp — see deck_app.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/deck_app.h"

#include "ocp.h"
#include "ui/link_view.h"
#include "ui/sweep_view.h"
#include "ui/trace_view.h"

namespace app {

namespace {
constexpr uint32_t kRetryMs = 2000;       /* reconnect while Disconnected */
constexpr uint32_t kKeepaliveMs = 3000;   /* ping when idle, to notice a lost probe */
constexpr uint32_t kRedrawMs = 200;
constexpr uint32_t kBusyRedrawMs = 500;   /* elapsed counter while scanning */
}  // namespace

DeckApp::DeckApp(ocp::Client::Write write) : client_(std::move(write))
{
    client_.onState([this](ocp::LinkState s) {
        log(std::string("state=") + ocp::linkStateName(s));
        dirty_ = true;
    });
    client_.onReset([this] {
        /* The probe's stored results died with it; so did our indices. */
        const auto &st = client_.stats();
        log("probe-reset resets=" + std::to_string(st.resets) + " noise=" + std::to_string(st.noise) +
            " stray=" + std::to_string(st.stray));
        scan_.clear();
        next_page_ = 0;
        if (screen_ != Screen::Link) screen_ = Screen::Sweep;
        notice("probe rebooted - resynced");
    });
    client_.onReply([this](const ocp::Item &it) { onReply(it); });
    client_.onEvent([this](const ocp::Item &) { dirty_ = true; });
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

void DeckApp::onReply(const ocp::Item &it)
{
    dirty_ = true;
    if (it.kind == ocp::ItemKind::Pong) { last_reply_ = "pong"; return; }

    if (it.kind == ocp::ItemKind::Error) {
        const auto *code = it.get(OCP_K_CODE);
        const auto *msg = it.get(OCP_K_MSG);
        notice(std::string("error ") + (code ? *code : "?") + ": " + (msg ? *msg : ""));
        log(std::string("err code=") + (code ? *code : "?"));
        return;
    }

    last_reply_ = it.tag;
    if (it.tag == OCP_MARK_SCAN) {
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

void DeckApp::startScan(uint32_t now_ms)
{
    if (client_.state() != ocp::LinkState::Ready) { notice("no probe"); return; }
    if (!client_.send(OCP_V_SCAN_NETWORKS, now_ms)) { notice("busy"); return; }
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
    trace_idx_ = scan_.rows()[cursor_].idx;
    if (!client_.send(std::string(OCP_V_INSPECT_NETWORK) + " " + std::to_string(trace_idx_), now_ms)) {
        notice("busy");
        return;
    }
    screen_ = Screen::Trace;
    notice("");
}

void DeckApp::back(uint32_t now_ms)
{
    if (client_.pending()) client_.stop(now_ms);
    screen_ = screen_ == Screen::Trace ? Screen::Sweep : Screen::Link;
    dirty_ = true;
}

void DeckApp::onKeys(const Keys &keys, uint32_t now_ms)
{
    now_ = now_ms;
    if (!keys.chars.empty() || keys.enter) log("keys=" + keys.chars + (keys.enter ? "<enter>" : ""));
    for (char c : keys.chars) {
        if (c == '`') { back(now_ms); continue; }

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
        }
        dirty_ = true;
    }
    if (keys.enter && screen_ == Screen::Sweep && !scan_.scanning()) startInspect(now_ms);
}

void DeckApp::tick(uint32_t now_ms)
{
    now_ = now_ms;
    client_.tick(now_ms);

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
    }
    dirty_ = false;
    last_draw_ms_ = now_ms;
}

}  // namespace app
