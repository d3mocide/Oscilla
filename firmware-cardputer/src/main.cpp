/*
 * main.cpp — oscilla-cp (the deck). Skeleton; transport and views land in P1-P2.
 *
 * Everything from the OCP client down stays framework-agnostic (DESIGN §7.1).
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp.h"

#include <Arduino.h>

#define OSCILLA_CP_VERSION "0.1.0"

/* Grove control UART, Rev D §3. Distinct from the GNSS UART (P4). */
static constexpr int kGroveTxPin = 2;
static constexpr int kGroveRxPin = 1;

void setup()
{
    Serial.begin(OCP_BAUD_DEFAULT);

    /* Placeholder: the line reader and resync logic are P1 work. */
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
