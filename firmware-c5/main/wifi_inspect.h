/*
 * wifi_inspect.h — passive beacon capture from one scanned AP (OCP-SPEC §10.3).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_INSPECT_H
#define OSCILLA_WIFI_INSPECT_H

#include "esp_err.h"

#define INSPECT_WINDOW_MS     2000   /* ~19 beacon intervals at the usual 102 ms */
#define INSPECT_ENOUGH        3      /* beacons that end the capture early */

esp_err_t wifi_inspect_init(void);

/* inspect_network <idx>: replies asynchronously with [INSPECT] or [ERR]. */
void wifi_cmd_inspect(int argc, char **argv);

#endif /* OSCILLA_WIFI_INSPECT_H */
