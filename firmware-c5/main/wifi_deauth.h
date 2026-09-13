/*
 * wifi_deauth.h — passive deauthentication/disassociation detector
 * (OCP-SPEC §10.5). Defensive only: DESIGN.md marks deauth *detection* as
 * in-scope receive-only monitoring, distinct from sending deauth frames
 * (an offensive, transmit-requiring action this project never does, D-8).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_DEAUTH_H
#define OSCILLA_WIFI_DEAUTH_H

#include "esp_err.h"

esp_err_t wifi_deauth_init(void);

/* deauth_detector: hops channels (wifi_channels.h), replies [CFG] with the
 * starting channel, then streams [EVT] kind=deauth for every deauth/disassoc
 * frame seen until `stop`. Every frame is reported (unlike the sniffer's
 * new-pairing-only events): occurrences, especially bursts, are the signal. */
void wifi_cmd_deauth_detector(void);

#endif /* OSCILLA_WIFI_DEAUTH_H */
