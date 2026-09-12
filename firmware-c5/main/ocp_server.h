/*
 * ocp_server.h — OCP command dispatch (OCP-SPEC.md §2).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_SERVER_H
#define OSCILLA_OCP_SERVER_H

#include "esp_err.h"

/* Starts the dispatch task and announces the probe with an unsolicited
 * [HELLO]. Priority sits above engine tasks so `stop` always lands. */
esp_err_t ocp_server_start(void);

/* Capabilities this build advertises, comma-separated; "" while no radio
 * engine exists. */
const char *ocp_server_caps(void);

#endif /* OSCILLA_OCP_SERVER_H */
