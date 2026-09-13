/*
 * main.cpp — oscilla-cp (the deck). Wiring only: M5 hardware, the Grove link,
 * keyboard and the app.
 *
 * SPDX-License-Identifier: MIT
 */

#include <M5Cardputer.h>

#include "app/deck_app.h"
#include "ocp.h"

namespace {

/* Rev D §3. Start Serial1 after M5.begin(): Port A shares these pins. */
constexpr int kGroveTxPin = 2;
constexpr int kGroveRxPin = 1;

app::DeckApp g_app([](const char *data, size_t len) {
    Serial1.write(reinterpret_cast<const uint8_t *>(data), len);
});

}  // namespace

void setup()
{
    Serial.begin(115200);   /* USB: deck diagnostics only */
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);

    /* A 256-row [SCAN] page is ~20 KB in ~2 s; a redraw must not overflow the buffer. */
    Serial1.setRxBufferSize(16384);
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);

    g_app.onLog([](const std::string &line) { Serial.printf("deck %s\n", line.c_str()); });
    g_app.begin(millis());
}

void loop()
{
    uint32_t now = millis();

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
