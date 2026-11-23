/*
 * cc1101_bus.h — CC1101 SPI access layer, receive-only (DESIGN §6.1).
 *
 * Register facts are from TI SWRS061I. The transport is injected, so this
 * layer owns no pins (pins come from Rev D only) and runs on the host
 * against a simulated chip. Every wait is bounded (Rev D §4.3).
 *
 * Receive-only (D-8): only an allowlist of strobes can be issued, and no
 * write can reach PATABLE or the TX FIFO.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_BUS_H
#define OSCILLA_CC1101_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Configuration registers used by name (SWRS061I Table 43). */
#define CC1101_IOCFG2    0x00
#define CC1101_IOCFG1    0x01
#define CC1101_IOCFG0    0x02
#define CC1101_CHANNR    0x0A
#define CC1101_FREQ2     0x0D
#define CC1101_FREQ1     0x0E
#define CC1101_FREQ0     0x0F
#define CC1101_MCSM0     0x18
#define CC1101_FSTEST    0x29
#define CC1101_PTEST     0x2A
#define CC1101_AGCTEST   0x2B
#define CC1101_CFG_COUNT 0x2F   /* 0x00..0x2E */

/* Status registers, read with the burst bit set (SWRS061I §10.2, Table 44). */
#define CC1101_PARTNUM   0x30
#define CC1101_VERSION   0x31
#define CC1101_RSSI      0x34
#define CC1101_MARCSTATE 0x35
#define CC1101_RXBYTES   0x3B

/* Receive-side command strobes (SWRS061I Table 42). Nothing else is sendable. */
#define CC1101_SRES  0x30
#define CC1101_SCAL  0x33
#define CC1101_SRX   0x34
#define CC1101_SIDLE 0x36
#define CC1101_SPWD  0x39
#define CC1101_SFRX  0x3A
#define CC1101_SNOP  0x3D

/* MARCSTATE values (SWRS061I §29.3). SLEEP (0x00) can never be read back. */
#define CC1101_MARC_IDLE 0x01
#define CC1101_MARC_RX   0x0D

#define CC1101_POR_FREQ2 0x1E   /* FREQ2 reset value (SWRS061I §29.1) */

typedef enum {
    CC1101_OK = 0,
    CC1101_ERR_ARG,          /* address/strobe outside the receive-only surface */
    CC1101_ERR_BUS,          /* transport reported a failure */
    CC1101_ERR_NOT_READY,    /* SO stayed high past the bound: XOSC not running */
    CC1101_ERR_NO_CHIP,      /* reads are impossible values: dead or floating bus */
    CC1101_ERR_UNSTABLE,     /* a status register never read the same twice */
    CC1101_ERR_RESET,        /* reset did not return the chip to POR/IDLE */
    CC1101_ERR_VERIFY,       /* a write did not read back */
    CC1101_ERR_STATE,        /* the state machine did not reach the requested state */
} cc1101_err_t;

/* Injected transport. CSn is driven separately so a transaction can hold it. */
typedef struct {
    void *ctx;
    bool (*xfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    void (*cs)(void *ctx, bool asserted);
    int (*so)(void *ctx);                 /* SO/MISO level, read as a GPIO */
    void (*delay_us)(void *ctx, uint32_t us);
    uint32_t (*now_us)(void *ctx);        /* monotonic, wraps */
} cc1101_bus_t;

/* Bound on CHIP_RDYn after CSn low; covers XOSC start (SWRS061I Table 22). */
#define CC1101_READY_TIMEOUT_US 2000u

const char *cc1101_err_name(cc1101_err_t err);

/* Single config-register access, 0x00..0x2E. Writes to FSTEST/PTEST/AGCTEST
 * are refused ("for test only", SWRS061I §29.2). */
cc1101_err_t cc1101_read_reg(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out);
cc1101_err_t cc1101_write_reg(const cc1101_bus_t *bus, uint8_t addr, uint8_t value);

/* Status register read, repeated until two consecutive reads agree
 * (SWRS061I §10.3 read-corruption note). */
cc1101_err_t cc1101_read_status(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out);

/* One status read, no agreement check: for continuously changing values
 * such as RSSI, where two equal reads cannot be expected. */
cc1101_err_t cc1101_read_status_raw(const cc1101_bus_t *bus, uint8_t addr, uint8_t *out);

/* Allowlisted strobe; *status (may be NULL) gets the chip status byte. */
cc1101_err_t cc1101_strobe(const cc1101_bus_t *bus, uint8_t strobe, uint8_t *status);

/* PARTNUM/VERSION, plus a MARCSTATE read that rejects the all-zero dead bus
 * that a passing CHIP_RDYn check cannot distinguish. */
cc1101_err_t cc1101_probe(const cc1101_bus_t *bus, uint8_t *partnum, uint8_t *version);

/* Manual reset (SWRS061I §19.1.2). The post-SRES SO edge is waited on with a
 * bound but not required; success is FREQ2 == POR and MARCSTATE == IDLE.
 * *so_edge (may be NULL) reports whether that edge was seen. */
cc1101_err_t cc1101_reset(const cc1101_bus_t *bus, bool *so_edge);

/* Poll MARCSTATE until it equals want or timeout_us passes. */
cc1101_err_t cc1101_wait_state(const cc1101_bus_t *bus, uint8_t want, uint32_t timeout_us,
                               uint8_t *last);

#endif /* OSCILLA_CC1101_BUS_H */
