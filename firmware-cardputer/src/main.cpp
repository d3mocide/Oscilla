/*
 * main.cpp — oscilla-cp (the deck).
 *
 * Skeleton only. P0 requires that this builds and compiles against the shared
 * OCP contract; the transport layer, connection state machine and views land
 * in P1–P2 (see ROADMAP.md).
 *
 * Layering (DESIGN §7.1): everything from the OCP client downward is
 * framework-agnostic plain C++, so a future LVGL move touches only views.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp.h"

#include <Arduino.h>

#define OSCILLA_CP_VERSION "0.1.0"

/*
 * Grove control UART — Rev D §3. Deck TX GPIO2 -> probe RX; deck RX GPIO1 <-
 * probe TX. A different hardware UART from the GNSS link (GPIO13/15), which
 * P4 brings up.
 */
static constexpr int kGroveTxPin = 2;
static constexpr int kGroveRxPin = 1;

void setup()
{
    Serial.begin(OCP_BAUD_DEFAULT);

    /* Not yet the real transport layer — the line reader, marker recogniser
     * and resync logic are P1 work. This is here so the skeleton demonstrably
     * compiles against the contract. */
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);
    Serial1.print(OCP_V_HELLO);
    Serial1.print(OCP_LINE_TERM);

    Serial.printf("%s proto=%d ver=%s\n",
                  OCP_FW_NAME_DECK, OCP_PROTO_VERSION, OSCILLA_CP_VERSION);
}

void loop()
{
    delay(1000);
}
