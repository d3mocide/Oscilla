/*
 * cc1101_port.h — ESP-IDF transport for cc1101_bus on the shared sub-GHz
 * SPI bus (DESIGN §6.1). CSn on C5 GPIO7 / D3 (Rev D §4.4).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_PORT_H
#define OSCILLA_CC1101_PORT_H

#include "cc1101_bus.h"
#include "esp_err.h"

/* Bring up the bus if needed, add the CC1101 device, drive CSn high, and
 * fill *bus. Safe to call once at boot after lora_radio_init(). */
esp_err_t cc1101_port_init(cc1101_bus_t *bus);

#endif /* OSCILLA_CC1101_PORT_H */
