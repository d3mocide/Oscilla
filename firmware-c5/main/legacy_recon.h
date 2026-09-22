/*
 * legacy_recon.h — CC1101 OCP verb glue (DESIGN §6.1).
 *
 * Owns nothing electrical: all SPI/GPIO work lives in cc1101_radio.c. This
 * file only translates OCP verbs <-> cc1101_radio's typed API, arbitrates
 * against the Wio over subghz_arbiter, and drains cc1101_radio's event
 * queue into [EVT] kind=legacy lines.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_LEGACY_RECON_H
#define OSCILLA_LEGACY_RECON_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t legacy_recon_init(void);
bool legacy_recon_ready(void);

void legacy_cmd_config(int argc, char **argv);
void legacy_cmd_listen(void);
void legacy_cmd_status(void);
bool legacy_cmd_stop(void);

#endif /* OSCILLA_LEGACY_RECON_H */
