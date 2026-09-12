/*
 * main.cpp — oscilla-cp (the deck). Wiring only: M5 hardware, the Grove link,
 * the OCP client, and the link view.
 *
 * SPDX-License-Identifier: MIT
 */

#include <M5Cardputer.h>

#include "ocp.h"
#include "ocp/ocp_client.h"
#include "ui/link_view.h"

namespace {

/* Rev D §3. Start Serial1 after M5.begin(): Port A shares these pins. */
constexpr int kGroveTxPin = 2;
constexpr int kGroveRxPin = 1;

constexpr uint32_t kRetryMs = 2000;       /* reconnect while Disconnected */
constexpr uint32_t kKeepaliveMs = 3000;   /* ping when idle, to notice a lost probe */
constexpr uint32_t kRedrawMs = 200;

std::string g_last_reply = "-";
std::string g_notice;
bool g_dirty = true;

ocp::Client g_client([](const char *data, size_t len) {
    Serial1.write(reinterpret_cast<const uint8_t *>(data), len);
});

std::string summarise(const ocp::Item &it)
{
    if (it.kind == ocp::ItemKind::Pong) return "pong";
    std::string s = it.kind == ocp::ItemKind::Error ? "[ERR]" : it.tag;
    for (const auto &e : it.kv) {
        s += " " + e.key + "=" + e.value;
        if (s.size() > 40) break;
    }
    return s;
}

void handleKey(char key, uint32_t now)
{
    const char *cmd = nullptr;
    switch (key) {
        case 'h': g_client.connect(now); g_notice = "hello"; g_dirty = true; return;
        case 'p': cmd = OCP_V_PING; break;
        case 's': cmd = OCP_V_STATUS; break;
        case 'v': cmd = OCP_V_VERSION; break;
        case 'r': cmd = OCP_V_REBOOT; break;
        case '`': cmd = OCP_V_STOP; break;   /* global stop (DESIGN §7.3) */
        default: return;
    }
    g_notice = g_client.send(cmd, now) ? std::string("> ") + cmd
                                       : std::string("busy: ") + cmd + " not sent";
    g_dirty = true;
}

}  // namespace

void setup()
{
    Serial.begin(115200);   /* USB: deck diagnostics only */
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);

    Serial1.setRxBufferSize(1024);
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);

    g_client.onState([](ocp::LinkState s) {
        Serial.printf("deck state=%s\n", ocp::linkStateName(s));
        g_dirty = true;
    });
    g_client.onReset([] {
        const auto &st = g_client.stats();
        Serial.printf("deck probe-reset resets=%u noise=%u stray=%u errors=%u\n",
                      st.resets, st.noise, st.stray, st.errors);
        g_notice = "probe rebooted - resynced";
    });
    g_client.onReply([](const ocp::Item &it) {
        g_last_reply = summarise(it);
        Serial.printf("deck reply %s\n", ui::printable(g_last_reply, 80).c_str());
        g_dirty = true;
    });
    g_client.onEvent([](const ocp::Item &) { g_dirty = true; });

    g_client.connect(millis());
}

void loop()
{
    static uint32_t last_attempt = 0, last_keepalive = 0, last_draw = 0;
    static uint32_t last_timeouts = 0;
    uint32_t now = millis();

    uint8_t buf[256];
    size_t n = 0;
    while (Serial1.available() && n < sizeof buf) buf[n++] = Serial1.read();
    if (n) g_client.feed(buf, n, now);
    g_client.tick(now);

    if (g_client.stats().timeouts != last_timeouts) {
        last_timeouts = g_client.stats().timeouts;
        Serial.printf("deck timeout total=%u\n", last_timeouts);
        g_dirty = true;
    }

    if (g_client.state() == ocp::LinkState::Disconnected && now - last_attempt >= kRetryMs) {
        last_attempt = now;
        g_client.connect(now);
    }
    if (g_client.state() == ocp::LinkState::Ready && !g_client.pending() &&
        now - last_keepalive >= kKeepaliveMs) {
        last_keepalive = now;
        g_client.send(OCP_V_PING, now);
    }

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        for (char c : M5Cardputer.Keyboard.keysState().word) handleKey(c, now);
    }

    if (g_dirty && now - last_draw >= kRedrawMs) {
        ui::drawLinkView(g_client, g_last_reply, g_notice);
        g_dirty = false;
        last_draw = now;
    }

    if (!n) delay(1);   /* yield when idle */
}
