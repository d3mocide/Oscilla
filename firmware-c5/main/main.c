/*
 * main.c — oscilla-c5 (the probe). Skeleton; engines land in P1-P3.
 *
 * Boot order once populated (DESIGN §6.1):
 *   NVS -> arbiter -> platform -> OCP server
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp.h"

#include <stdio.h>

/* Reported by `version` and the [HELLO] handshake. */
#define OSCILLA_C5_VERSION "0.1.0"

/* Caps this build advertises; all receive-only (DESIGN §8). Added per phase. */
#define OSCILLA_C5_CAPS ""   /* P2 adds wifi24/wifi5; P3 lora_rx; P7 ble/802.15.4 */

void app_main(void)
{
    /* Placeholder, not the real handshake: the OCP server is P1 work. */
    printf("%s " OCP_K_PROTO "=%d " OCP_K_FW "=%s " OCP_K_VER "=%s "
           OCP_K_CAPS "=%s " OCP_KW_END "\n",
           OCP_MARK_HELLO, OCP_PROTO_VERSION, OCP_FW_NAME_PROBE,
           OSCILLA_C5_VERSION, OSCILLA_C5_CAPS);
}
