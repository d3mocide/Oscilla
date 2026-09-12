/*
 * main.c — oscilla-c5 (the probe).
 *
 * Skeleton only. P0 requires that this builds and compiles against the shared
 * OCP contract; the OCP server, the radio arbiter and the recon engines land
 * in P1–P3 (see ROADMAP.md).
 *
 * Boot order once populated (DESIGN §6.1):
 *   NVS -> arbiter -> platform (netif/event/esp_wifi NULL) -> OCP server
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp.h"

#include <stdio.h>

/* The firmware version reported by `version` and the [HELLO] handshake. */
#define OSCILLA_C5_VERSION "0.1.0"

/*
 * Capabilities this build advertises. Every one names a RECEIVE capability —
 * there is no transmit cap because there is no transmit verb (DESIGN §8).
 * Entries are added as their phases land; the handshake is the only place the
 * deck learns what this probe can do.
 */
#define OSCILLA_C5_CAPS ""   /* P2 adds wifi24/wifi5; P3 lora_rx; P7 ble/802.15.4 */

void app_main(void)
{
    /* Not yet the real handshake — the OCP server is P1 work. This is here so
     * the skeleton demonstrably compiles against the contract. */
    printf("%s " OCP_K_PROTO "=%d " OCP_K_FW "=%s " OCP_K_VER "=%s "
           OCP_K_CAPS "=%s " OCP_KW_END "\n",
           OCP_MARK_HELLO, OCP_PROTO_VERSION, OCP_FW_NAME_PROBE,
           OSCILLA_C5_VERSION, OSCILLA_C5_CAPS);
}
