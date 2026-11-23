/*
 * subghz_spi.h — the SPI bus shared by the Wio-SX1262 and the CC1101
 * (Rev D §4.1, CC1101 addendum §2). Owns the bus pins and one lock that
 * every driver holds for its whole chip-select-low span, so neither chip
 * ever sees the other's clocking while selected.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_SUBGHZ_SPI_H
#define OSCILLA_SUBGHZ_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"

#define SUBGHZ_SPI_HOST SPI2_HOST
#define SUBGHZ_PIN_SCK  8    /* D8 */
#define SUBGHZ_PIN_MISO 9    /* D9: SX1262 MISO and CC1101 SO/GDO1 */
#define SUBGHZ_PIN_MOSI 10   /* D10 */

/* Bound on waiting for the other radio's transaction (Rev D §4.3). */
#define SUBGHZ_SPI_LOCK_TIMEOUT_MS 50

/* Initialise the bus once; later calls succeed without re-initialising.
 * max_transfer applies only to the first call. */
esp_err_t subghz_spi_bus_init(size_t max_transfer);

/* Hold for the entire CS-low span. False on timeout: do not select. */
bool subghz_spi_lock(uint32_t timeout_ms);
void subghz_spi_unlock(void);

#endif /* OSCILLA_SUBGHZ_SPI_H */
