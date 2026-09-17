/*
 * client_test.cpp — host test for ocp::Client against a scripted probe.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>
#include <vector>

#include "ocp/ocp_client.h"

namespace {

int g_failed = 0, g_passed = 0;

void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_passed : g_failed)++;
}

struct Rig {
    std::string wire;                       /* what the deck wrote */
    std::vector<ocp::Item> replies, events;
    std::vector<ocp::LinkState> states;
    int resets = 0;
    uint32_t now = 1000;
    ocp::Client c;

    Rig() : c([this](const char *d, size_t n) { wire.append(d, n); })
    {
        c.onReply([this](const ocp::Item &it) { replies.push_back(it); });
        c.onEvent([this](const ocp::Item &it) { events.push_back(it); });
        c.onState([this](ocp::LinkState s) { states.push_back(s); });
        c.onReset([this] { resets++; });
    }
    void probe(const std::string &s) { c.feed((const uint8_t *)s.data(), s.size(), now); }
    void advance(uint32_t ms) { now += ms; c.tick(now); }
    std::string takeWire() { std::string w; w.swap(wire); return w; }
};

/* Real ROM text from the C5 over Grove (docs/hardware/link-bringup.md). */
constexpr const char *kBootText =
    "ESP-ROM:esp32c5-eco2-20250121\r\n"
    "Build:Jan 21 2025\r\n"
    "rst:0xc (SW_CPU),boot:0x18 (SPI_FAST_FLASH_BOOT)\r\n"
    "Core0 Saved PC:0x408069ee\r\n"
    "SPI mode:DIO, clock div:1\r\n"
    "load:0x408556c0,len:0x1714\r\n"
    "entry 0x4084bbaa\r\n";

constexpr const char *kHello = "[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,ble END\n";

}  // namespace

int main()
{
    using S = ocp::LinkState;

    {
        Rig r;
        check(r.c.state() == S::Disconnected, "starts Disconnected");
        check(!r.c.send("ping", r.now), "no commands before the handshake");

        r.c.connect(r.now);
        check(r.takeWire() == "\nhello\n" && r.c.state() == S::HelloSent,
              "connect flushes a partial line, then sends hello");

        r.probe(kHello);
        check(r.c.state() == S::Ready, "[HELLO] -> Ready");
        check(r.c.probe().fw == "oscilla-c5" && r.c.probe().ver == "0.1.0" &&
              r.c.probe().caps == "wifi24,ble" && r.c.probe().proto == 1, "probe info parsed");
        check(r.resets == 0, "a solicited [HELLO] is not a reset");

        check(r.c.send("ping", r.now) && r.takeWire() == "ping\n", "ping sent");
        check(!r.c.send("status", r.now), "one command at a time");
        r.probe("pong\n");
        check(!r.c.pending() && r.replies.back().kind == ocp::ItemKind::Pong, "pong answers ping");

        r.c.send("status", r.now);
        r.probe("[EVT] kind=sniff pkts=3\n[STATUS] owner=none uptime_ms=5 END\n");
        check(r.events.size() == 1, "event routed to the event sink");
        check(!r.c.pending() && r.replies.back().tag == "[STATUS]", "[STATUS] answers status");

        /* Regression for P7: a bare [CFG] END is a late terminator, not an
         * empty acknowledgement. The valid zero-row block must complete the
         * same start command. */
        r.c.send("start_antisurveillance", r.now);
        r.probe("[CFG] END\n");
        check(r.c.pending() && r.c.stats().noise > 0,
              "bare empty CFG is noise and cannot complete a start");
        r.advance(ocp::Client::kReplyTimeoutMs);
        check(!r.c.pending() && r.c.state() == S::Ready,
              "a malformed CFG start times out without disconnecting the link");
        r.c.send("start_antisurveillance", r.now);
        r.probe("[CFG] BEGIN\n[CFG] END\n");
        check(!r.c.pending() && r.replies.back().tag == "[CFG]",
              "valid zero-row CFG block completes the start");

        r.c.send("version", r.now);
        r.probe("[SCAN] BEGIN\n[SCAN] END\n");
        check(r.c.pending() && r.c.stats().stray == 1, "a frame for nothing pending is stray, not a reply");
        r.probe("[VER] fw=oscilla-c5 ver=0.1.0 END\n");
        check(!r.c.pending(), "the right frame still answers afterwards");

        r.c.send("scan_networks", r.now);
        r.probe("[ERR] code=nocap msg=\"no\"\n");
        check(!r.c.pending() && r.replies.back().kind == ocp::ItemKind::Error, "[ERR] answers the pending command");

        r.takeWire();
        check(!r.c.send("transmit_now", r.now) && r.wire.empty(), "a verb outside the contract never reaches the wire");
    }

    {
        Rig r;
        r.c.connect(r.now); r.probe(kHello); r.takeWire();

        /* Probe resets mid-command and mid-frame: boot text, then [HELLO]. */
        r.c.send("status", r.now);
        r.probe("[LORA] BEGIN state=idle\n[LORA] freq=0\n");
        r.probe(kBootText);
        r.probe(kHello);
        check(r.resets == 1 && r.c.stats().resets == 1, "unsolicited [HELLO] counts as a probe reset");
        check(!r.c.pending(), "the in-flight command is cancelled");
        check(r.c.state() == S::Ready, "back to Ready after the reset");
        check(r.c.stats().noise >= 7, "every boot line is noise");
        check(r.c.stats().stray == 0 && r.replies.back().tag == "[HELLO]", "no boot line was taken for a frame");
        check(r.c.send("ping", r.now), "usable immediately after the reset");
    }

    {
        Rig r;
        r.c.connect(r.now); r.probe(kHello);

        r.c.send("version", r.now);   /* generic timeout; scans have their own */
        r.advance(ocp::Client::kReplyTimeoutMs - 1);
        check(r.c.pending(), "not timed out one ms early");
        r.advance(1);
        check(!r.c.pending() && r.c.stats().timeouts == 1 && r.c.state() == S::Ready,
              "slow command times out to Ready (OCP-SPEC §5.2)");

        r.c.send("ping", r.now);
        r.advance(ocp::Client::kReplyTimeoutMs);
        check(r.c.state() == S::Disconnected, "unanswered ping -> Disconnected");

        r.c.connect(r.now);
        r.advance(ocp::Client::kHelloTimeoutMs);
        check(r.c.state() == S::Disconnected, "unanswered hello -> Disconnected");
    }

    {
        Rig r;
        r.c.connect(r.now); r.probe(kHello); r.takeWire();
        check(r.c.send("reboot", r.now) && r.takeWire() == "reboot\n", "reboot sent");
        check(r.c.state() == S::HelloSent, "reboot waits for the probe to come back");
        r.advance(3000);
        r.probe(kBootText); r.probe(kHello);
        check(r.c.state() == S::Ready && r.resets == 0, "expected [HELLO] after reboot: Ready, not a surprise reset");

        r.c.send("reboot", r.now);
        r.advance(ocp::Client::kRebootTimeoutMs);
        check(r.c.state() == S::Disconnected, "probe that never returns from reboot -> Disconnected");
    }

    {
        Rig r;
        r.c.connect(r.now); r.probe(kHello); r.takeWire();

        r.c.send("scan_networks", r.now);
        r.advance(ocp::Client::kReplyTimeoutMs + 100);
        check(r.c.pending() && r.c.stats().timeouts == 0, "a passive scan outlives the generic 2 s timeout");
        r.advance(ocp::Client::kScanTimeoutMs);
        check(!r.c.pending() && r.c.stats().timeouts == 1 && r.c.state() == S::Ready,
              "but still times out, to Ready, at the scan timeout");

        /* stop while a scan is pending (OCP-SPEC §2.1). */
        r.c.send("scan_networks", r.now); r.takeWire();
        check(r.c.send("stop", r.now) && r.takeWire() == "stop\n", "stop is sent despite a pending command");
        check(r.c.stopPending() && r.c.pending(), "scan and stop both pending");
        check(!r.c.stop(r.now), "a second stop is not sent while one is pending");
        size_t before = r.replies.size();
        r.probe("[SCAN] BEGIN n=0 total=0 first=1 aborted=1\n[SCAN] END\n[STOP] running=1 END\n");
        check(!r.c.pending() && !r.c.stopPending(), "aborted [SCAN] answers the scan, [STOP] answers the stop");
        check(r.replies.size() == before + 2 && r.c.stats().stray == 0, "two replies, nothing stray");

        r.c.stop(r.now);
        r.advance(ocp::Client::kStopTimeoutMs);
        check(r.c.state() == S::Disconnected, "a stop nobody answers -> Disconnected");
    }

    {
        /* Scoped stop (D-16, OCP-SPEC §5.4). The lane must reach the wire,
         * and a scoped ack must not be mistaken for a global one. */
        Rig r;
        r.c.connect(r.now); r.probe(kHello); r.takeWire();

        check(r.c.stop(r.now) && r.takeWire() == "stop\n",
              "an unscoped stop stays byte-identical to a pre-D-16 deck");
        r.probe("[STOP] lane=all running=1 END\n");
        check(!r.c.stopPending(), "[STOP] lane=all answers it");

        check(r.c.stop(r.now, OCP_LANE_PHY) && r.takeWire() == "stop phy\n",
              "stop(phy) puts the lane on the wire");
        r.probe("[STOP] lane=phy running=1 END\n");
        check(!r.c.stopPending() && r.c.stats().stray == 0, "[STOP] lane=phy answers it");

        check(r.c.send("stop lora", r.now) && r.takeWire() == "stop lora\n",
              "send(\"stop lora\") routes the lane through to stop()");
        check(r.c.stopPending(), "a scoped stop is still tracked as pending");
        r.probe("[STOP] lane=lora running=0 END\n");
        check(!r.c.stopPending() && r.c.stats().stray == 0, "[STOP] lane=lora answers it");

        /* A probe that refuses the lane must not leave the deck waiting. */
        check(r.c.stop(r.now, OCP_LANE_LORA), "a stop after a completed one is allowed");
        r.advance(ocp::Client::kStopTimeoutMs);
        check(r.c.state() == S::Disconnected, "an unanswered scoped stop still times out");
    }

    {
        /* Seen on hardware: junk from an esptool reset, ended by our flush
         * newline, draws [ERR] unknown before our hello is answered. */
        Rig r;
        r.c.connect(r.now);
        r.probe("[ERR] code=unknown msg=\"unknown verb\"\n");
        check(r.c.pending() && r.c.state() == S::HelloSent, "an [ERR] does not cancel a pending hello");
        r.probe(kHello);
        check(r.c.state() == S::Ready, "the real [HELLO] still completes the handshake");
    }

    {
        Rig r;
        r.c.connect(r.now);
        r.probe("[HELLO] proto=2 fw=oscilla-c5 ver=9.0.0 caps= END\n");
        check(r.c.state() == S::Incompatible, "unknown proto -> Incompatible (§4)");
        check(!r.c.send("ping", r.now), "no commands to an incompatible probe");
    }

    {
        Rig r;
        r.c.connect(r.now); r.probe(kHello);
        uint32_t wrap = 0xFFFFFFFFu - 500;
        r.now = wrap;
        r.c.send("status", r.now);
        r.advance(1000);   /* crosses the 32-bit millis wrap */
        check(r.c.pending() && r.c.stats().timeouts == 0, "no false timeout across the millis() wrap");
        r.advance(ocp::Client::kReplyTimeoutMs - 1000);
        check(!r.c.pending() && r.c.stats().timeouts == 1, "real timeout still fires after the wrap");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_failed ? "client test FAILED" : "client test OK",
                g_passed, g_failed);
    return g_failed ? 1 : 0;
}
