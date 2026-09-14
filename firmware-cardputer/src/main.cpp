/*
 * main.cpp — oscilla-cp (the deck). Wiring only: M5 hardware, the Grove link,
 * keyboard and the app.
 *
 * SPDX-License-Identifier: MIT
 */

#include <M5Cardputer.h>

#include "app/deck_app.h"
#include "gnss/nmea_parser.h"
#include "model/gnss_model.h"
#include "ocp.h"
#include "storage/sd_storage.h"
#include "ui/canvas.h"

namespace {

/* Rev D §3. Start Serial1 after M5.begin(): Port A shares these pins. */
constexpr int kGroveTxPin = 2;
constexpr int kGroveRxPin = 1;

/* Rev D §6: a distinct hardware UART from Grove. TX 13 / RX 15 are the
 * host's own pins; 9600 8N1 is the -5N family default, not guaranteed for a
 * preconfigured unit — no settings UI yet to change it at runtime, so this
 * is the one place to edit until P4 grows one. */
constexpr int kGnssTxPin = 13;
constexpr int kGnssRxPin = 15;
constexpr uint32_t kGnssBaudDefault = 9600;

app::DeckApp g_app([](const char *data, size_t len) {
    Serial1.write(reinterpret_cast<const uint8_t *>(data), len);
});

gnss::NmeaParser g_gnss_parser;
model::GnssModel g_gnss;

}  // namespace

void setup()
{
    Serial.begin(115200);   /* USB: deck diagnostics only */
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);
    ui::initCanvas();

    /* Brought up before any TFT traffic exists to contend with it, per
     * DESIGN §7.4 — the external TFT isn't wired yet, but when it is, SD
     * still needs to win this race. A missing/unreadable card just means
     * logging is unavailable this boot; never block startup on it. */
    if (!storage::begin()) Serial.println("deck: no SD card, logging unavailable");

    /* A 256-row [SCAN] page is ~20 KB in ~2 s; a redraw must not overflow the buffer. */
    Serial1.setRxBufferSize(16384);
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);

    /* Own UART, own baud: GNSS never crosses OCP (DESIGN §9.1), so it has
     * nothing to do with the Grove link's proto or framing. */
    Serial2.begin(kGnssBaudDefault, SERIAL_8N1, kGnssRxPin, kGnssTxPin);

    g_app.onLog([](const std::string &line) { Serial.printf("deck %s\n", line.c_str()); });
    g_app.begin(millis());
}

namespace {
/* Diagnostic only, not tied to any view: a slow leak shows up as a trend in
 * this number over hours, not as a crash. Independent of which screen is
 * shown, since info_view's own heap readout only samples while you're
 * looking at it. */
constexpr uint32_t kHeapLogMs = 5UL * 60UL * 1000UL;
uint32_t g_last_heap_log_ms = 0;
}  // namespace

void loop()
{
    uint32_t now = millis();

    if (now - g_last_heap_log_ms >= kHeapLogMs) {
        g_last_heap_log_ms = now;
        Serial.printf("deck heap=%u uptime_s=%lu\n", (unsigned)ESP.getFreeHeap(), (unsigned long)(now / 1000));
    }

    /* Drain the link completely before any drawing. */
    uint8_t buf[512];
    size_t n = 0, total = 0;
    do {
        n = 0;
        while (Serial1.available() && n < sizeof buf) buf[n++] = Serial1.read();
        if (n) g_app.feed(buf, n, now);
        total += n;
    } while (n == sizeof buf);
    g_app.tick(now);

    /* GNSS: same drain-completely-before-drawing shape, independent UART.
     * No view reads g_gnss yet — Rev D's own bring-up step (outdoor fix,
     * antenna-unplugged no-fix check) is P4's hardware half, not this one. */
    while (Serial2.available()) {
        uint8_t gbuf[128];
        size_t gn = 0;
        while (Serial2.available() && gn < sizeof gbuf) gbuf[gn++] = Serial2.read();
        g_gnss_parser.feed(gbuf, gn, [&](const gnss::Sentence &s) { g_gnss.absorb(s, now); });
    }

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const auto &ks = M5Cardputer.Keyboard.keysState();
        app::Keys keys;
        keys.chars.assign(ks.word.begin(), ks.word.end());
        keys.enter = ks.enter;
        g_app.onKeys(keys, now);
    }

    if (g_app.dirty(now)) g_app.draw(now);

    if (!total) delay(1);   /* yield when idle */
}
