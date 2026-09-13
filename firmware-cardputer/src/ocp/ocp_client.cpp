/*
 * ocp_client.cpp — see ocp_client.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_client.h"

#include <cstdlib>
#include <cstring>

#include "ocp.h"

namespace ocp {

namespace {

struct VerbReply {
    const char *verb;
    const char *reply;
};

#define X(id, verb, cc, mn, mx, reply) { verb, reply },
constexpr VerbReply k_replies[] = { OCP_VERB_TABLE(X) };
#undef X

const char *expectedReply(const std::string &verb)
{
    for (const auto &v : k_replies) {
        if (verb == v.verb) return v.reply;
    }
    return nullptr;
}

uint32_t replyTimeout(const std::string &verb)
{
    if (verb == OCP_V_SCAN_NETWORKS)   return Client::kScanTimeoutMs;
    if (verb == OCP_V_INSPECT_NETWORK) return Client::kInspectTimeoutMs;
    return Client::kReplyTimeoutMs;
}

/* Liveness probes: if these time out, the probe isn't there. */
bool isLiveness(const std::string &verb)
{
    return verb == OCP_V_HELLO || verb == OCP_V_PING;
}

}  // namespace

const char *linkStateName(LinkState s)
{
    switch (s) {
        case LinkState::Disconnected: return "disconnected";
        case LinkState::HelloSent:    return "hello-sent";
        case LinkState::Ready:        return "ready";
        case LinkState::Incompatible: return "incompatible";
    }
    return "?";
}

Client::Client(Write write) : write_(std::move(write)) {}

void Client::setState(LinkState s)
{
    if (s == state_) return;
    state_ = s;
    if (on_state_) on_state_(s);
}

void Client::writeLine(const std::string &line)
{
    std::string out = line + OCP_LINE_TERM;
    write_(out.data(), out.size());
}

void Client::finishPending()
{
    pending_verb_.clear();
    pending_reply_ = "";
}

void Client::connect(uint32_t now_ms)
{
    finishPending();
    /* End any half-line a reset left in the probe's buffer, so it can't
     * prefix hello (blank lines are ignored, OCP-SPEC §2). */
    write_(OCP_LINE_TERM, 1);
    writeLine(OCP_V_HELLO);
    pending_verb_ = OCP_V_HELLO;
    pending_reply_ = OCP_MARK_HELLO;
    sent_at_ = now_ms;
    timeout_ms_ = kHelloTimeoutMs;
    setState(LinkState::HelloSent);
}

bool Client::stop(uint32_t now_ms)
{
    if (state_ != LinkState::Ready || stop_pending_) return false;
    writeLine(OCP_V_STOP);
    stop_pending_ = true;
    stop_sent_at_ = now_ms;
    return true;
}

bool Client::send(const std::string &line, uint32_t now_ms)
{
    std::string verb = line.substr(0, line.find(' '));
    if (verb == OCP_V_STOP) return stop(now_ms);
    if (state_ != LinkState::Ready || pending()) return false;

    const char *reply = expectedReply(verb);
    if (!reply) return false;   /* not in the contract: never put it on the wire */

    writeLine(line);
    sent_at_ = now_ms;

    if (verb == OCP_V_REBOOT) {
        /* No reply; the probe announces itself with [HELLO] when back. */
        pending_verb_ = OCP_V_HELLO;
        pending_reply_ = OCP_MARK_HELLO;
        timeout_ms_ = kRebootTimeoutMs;
        setState(LinkState::HelloSent);
        return true;
    }
    pending_verb_ = verb;
    pending_reply_ = reply;
    timeout_ms_ = replyTimeout(verb);
    return true;
}

void Client::feed(const uint8_t *data, size_t len, uint32_t now_ms)
{
    now_ = now_ms;
    parser_.feed(data, len, [this](Item &&it) { onItem(std::move(it)); });
}

void Client::tick(uint32_t now_ms)
{
    now_ = now_ms;
    if (stop_pending_ && now_ms - stop_sent_at_ >= kStopTimeoutMs) {
        stats_.timeouts++;
        stop_pending_ = false;
        setState(LinkState::Disconnected);   /* a probe that ignores stop is gone */
        return;
    }
    if (!pending() || now_ms - sent_at_ < timeout_ms_) return;   /* wrap-safe */

    stats_.timeouts++;
    parser_.abandonOpenFrame();
    bool dead = isLiveness(pending_verb_);
    finishPending();
    /* OCP-SPEC §5.2: a slow command leaves the link Ready; a liveness probe
     * that gets no answer means there is no probe. */
    if (dead) setState(LinkState::Disconnected);
}

void Client::handleHello(const Item &it)
{
    stats_.hellos++;
    bool solicited = pending_verb_ == OCP_V_HELLO;

    ProbeInfo info;
    if (const auto *v = it.get(OCP_K_PROTO)) info.proto = std::atoi(v->c_str());
    if (const auto *v = it.get(OCP_K_FW))    info.fw = *v;
    if (const auto *v = it.get(OCP_K_VER))   info.ver = *v;
    if (const auto *v = it.get(OCP_K_CAPS))  info.caps = *v;
    probe_ = info;

    /* Anything in flight will never be answered by a probe that rebooted. */
    finishPending();
    stop_pending_ = false;
    parser_.abandonOpenFrame();

    if (!solicited && state_ == LinkState::Ready) {
        stats_.resets++;
        if (on_reset_) on_reset_();
    }
    setState(info.proto == OCP_PROTO_VERSION ? LinkState::Ready : LinkState::Incompatible);
}

void Client::onItem(Item &&it)
{
    switch (it.kind) {
    case ItemKind::Noise:
        stats_.noise++;
        return;

    case ItemKind::Event:
        stats_.events++;
        if (on_event_) on_event_(it);
        return;

    case ItemKind::Error:
        stats_.errors++;
        /* hello exists in every probe: an [ERR] now answers something else,
         * e.g. junk the probe had buffered before our hello. */
        if (pending() && pending_verb_ != OCP_V_HELLO) {
            finishPending();
            if (on_reply_) on_reply_(it);
        }
        return;

    case ItemKind::Pong:
        if (pending_verb_ == OCP_V_PING) {
            finishPending();
            if (on_reply_) on_reply_(it);
        } else {
            stats_.stray++;
        }
        return;

    case ItemKind::Frame:
        if (it.tag == OCP_MARK_HELLO) {
            handleHello(it);
            if (on_reply_) on_reply_(it);
            return;
        }
        if (it.tag == OCP_MARK_STOP && stop_pending_) {
            stop_pending_ = false;
            if (on_reply_) on_reply_(it);
            return;
        }
        if (pending() && it.tag == pending_reply_) {
            finishPending();
            if (on_reply_) on_reply_(it);
        } else {
            stats_.stray++;
        }
        return;
    }
}

}  // namespace ocp
