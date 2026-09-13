/*
 * ocp_client.h — deck side of the OCP link: handshake, one-at-a-time
 * commands, reply/event routing, timeouts (OCP-SPEC §4-5). Framework-agnostic:
 * the caller supplies bytes, a clock, and a write function.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ocp_parser.h"

namespace ocp {

enum class LinkState : uint8_t {
    Disconnected,   /* no live probe */
    HelloSent,      /* waiting for [HELLO] */
    Ready,          /* handshake done; commands allowed */
    Incompatible,   /* probe speaks a proto this deck doesn't (§4) */
};

const char *linkStateName(LinkState s);

struct ProbeInfo {
    int proto = 0;
    std::string fw, ver, caps;
};

struct ClientStats {
    uint32_t hellos = 0;     /* [HELLO] frames seen */
    uint32_t resets = 0;     /* unsolicited [HELLO]: probe rebooted on its own */
    uint32_t timeouts = 0;
    uint32_t noise = 0;      /* lines ignored: boot text, garbage */
    uint32_t events = 0;
    uint32_t errors = 0;
    uint32_t stray = 0;      /* frames that answered nothing pending */
};

class Client {
public:
    static constexpr uint32_t kHelloTimeoutMs  = 1500;
    static constexpr uint32_t kReplyTimeoutMs  = 2000;
    static constexpr uint32_t kRebootTimeoutMs = 6000;
    static constexpr uint32_t kScanTimeoutMs   = 30000;   /* passive dual-band ~10.5 s */
    static constexpr uint32_t kInspectTimeoutMs = 6000;   /* 2 s capture + margin */
    static constexpr uint32_t kStopTimeoutMs   = 3000;

    using Write = std::function<void(const char *data, size_t len)>;
    using ItemSink = std::function<void(const Item &)>;
    using StateSink = std::function<void(LinkState)>;

    explicit Client(Write write);

    void onReply(ItemSink s) { on_reply_ = std::move(s); }   /* Frame, Pong or Error */
    void onEvent(ItemSink s) { on_event_ = std::move(s); }
    void onState(StateSink s) { on_state_ = std::move(s); }
    void onReset(std::function<void()> s) { on_reset_ = std::move(s); }

    /* Feed bytes read from the link. */
    void feed(const uint8_t *data, size_t len, uint32_t now_ms);

    /* Expire the pending command. Call every loop. */
    void tick(uint32_t now_ms);

    /* Send `hello` and wait for the handshake. */
    void connect(uint32_t now_ms);

    /* One command line, no terminator. False if not Ready or one is pending.
     * `stop` is routed to stop(). */
    bool send(const std::string &line, uint32_t now_ms);

    /* Allowed while a command is pending (OCP-SPEC §2.1): the cancelled
     * command still gets its aborted reply, then [STOP]. */
    bool stop(uint32_t now_ms);

    LinkState state() const { return state_; }
    bool pending() const { return !pending_verb_.empty(); }
    bool stopPending() const { return stop_pending_; }
    const std::string &pendingVerb() const { return pending_verb_; }
    const ProbeInfo &probe() const { return probe_; }
    const ClientStats &stats() const { return stats_; }

private:
    void setState(LinkState s);
    void onItem(Item &&it);
    void handleHello(const Item &it);
    void finishPending();
    void writeLine(const std::string &line);

    Write write_;
    Parser parser_;
    LinkState state_ = LinkState::Disconnected;
    ProbeInfo probe_;
    ClientStats stats_;

    std::string pending_verb_;
    const char *pending_reply_ = "";   /* expected marker, from OCP_VERB_TABLE */
    uint32_t sent_at_ = 0;
    uint32_t timeout_ms_ = 0;
    uint32_t now_ = 0;

    bool stop_pending_ = false;
    uint32_t stop_sent_at_ = 0;

    ItemSink on_reply_, on_event_;
    StateSink on_state_;
    std::function<void()> on_reset_;
};

}  // namespace ocp
