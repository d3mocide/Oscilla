/*
 * ocp_transport.h — byte I/O for the OCP link.
 *
 * Two backends, chosen by Kconfig: the Grove UART (the real link, Rev D §3)
 * or USB Serial/JTAG for bench work with no Grove cable.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_TRANSPORT_H
#define OSCILLA_OCP_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t ocp_transport_init(void);

/* Returns bytes read, 0 on timeout. Never blocks longer than timeout_ms. */
int ocp_transport_read(uint8_t *buf, size_t len, uint32_t timeout_ms);

void ocp_transport_write(const char *data, size_t len);

/* For [STATUS]; identifies which backend this build is using. */
const char *ocp_transport_name(void);

#endif /* OSCILLA_OCP_TRANSPORT_H */
