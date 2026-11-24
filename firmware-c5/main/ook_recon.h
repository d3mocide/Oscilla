/*
 * ook_recon.h — the OOK lane (D-20, DESIGN §6.2): CC1101 receive sessions
 * behind `ook_listen` / `ook_status` / `stop ook`, decoded packets as
 * [EVT] kind=ook and undecoded transmissions as kind=ook_unknown
 * (OCP-SPEC §15). Concurrent with the PHY and LoRa lanes. Receive-only: no
 * transmit-shaped entry point, and the driver below has none either.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OOK_RECON_H
#define OSCILLA_OOK_RECON_H

#include <stdbool.h>
#include "esp_err.h"

/* Bring the CC1101 up once at boot and park it powered down. A missing or
 * dead module is not an error: the lane is simply absent from caps. Call
 * after lora_recon_init() (shared SPI bus). */
esp_err_t ook_recon_init(void);

/* Caps are a promise (OCP-SPEC §4): true only when the chip answered,
 * reset, and took the profile. */
bool ook_recon_ready(void);
bool ook_recon_is_running(void);

void ook_cmd_listen(int argc, char **argv);
void ook_cmd_status(void);

/* Idempotent. True if a session was running. Returns with the chip idle. */
bool ook_cmd_stop(void);

#endif /* OSCILLA_OOK_RECON_H */
