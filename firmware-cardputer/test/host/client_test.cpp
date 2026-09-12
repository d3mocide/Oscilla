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
        check(r.takeWire() == "hello\n" && r.c.state() == S::HelloSent, "connect sends hello");

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

        r.c.send("scan_networks", r.now);
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
