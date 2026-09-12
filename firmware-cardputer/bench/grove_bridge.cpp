/*
 * grove_bridge.cpp — bench tool, not the deck app.
 *
 * Relays bytes between the Cardputer's USB port and the Grove UART, so host
 * tools (ocp_repl.py --gate) can drive the probe over the real link. No
 * parsing and no output of its own: anything it printed would land in the
 * command channel (Rev D §3).
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>

#include "ocp.h"

/* Rev D §3: S3 GPIO2 TX -> C5 GPIO12; S3 GPIO1 RX <- C5 GPIO11. */
static constexpr int kGroveTxPin = 2;
static constexpr int kGroveRxPin = 1;
static constexpr size_t kBufBytes = 1024;

static void relay(Stream &from, Stream &to, uint8_t *buf, bool &moved)
{
    int avail = from.available();
    if (avail <= 0) return;
    size_t n = from.readBytes(buf, min((size_t)avail, kBufBytes));
    to.write(buf, n);
    moved = true;
}

void setup()
{
    Serial.setRxBufferSize(kBufBytes);
    Serial.setTxBufferSize(kBufBytes);
    Serial.begin(OCP_BAUD_DEFAULT);
    Serial.setTxTimeoutMs(0);   /* host not reading: drop, never block */

    Serial1.setRxBufferSize(kBufBytes);
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);
}

void loop()
{
    static uint8_t buf[kBufBytes];
    bool moved = false;

    relay(Serial1, Serial, buf, moved);
    relay(Serial, Serial1, buf, moved);

    if (!moved) {
        delay(1);   /* yield when idle (AGENTS.md §5 gotcha 6) */
    }
}
