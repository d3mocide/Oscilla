/*
 * cc1101_sim.h — byte-level CC1101 SPI simulator for host tests, with the
 * fault modes seen on the bench (WORKLOG 2026-11-22/23).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_SIM_H
#define OSCILLA_CC1101_SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "cc1101_bus.h"

#define SIM_LOG 256

typedef struct {
    uint8_t reg[CC1101_CFG_COUNT];
    uint8_t marc;
    bool calibrated;
    uint32_t now_us;

    /* Faults. */
    bool dead_bus;              /* SO held low: every byte reads 0x00, writes lost */
    bool never_ready;           /* SO stuck high (XOSC not running) */
    int not_ready_after_txns;   /* never_ready from this transaction on (0 = off) */
    bool so_high_after_sres;    /* SO stays high while CSn is held after SRES */
    bool wedge_after_sres;      /* writes ignored after SRES (Koko, 2026-11-23) */
    bool sres_ignored;          /* SRES has no effect */
    bool ignore_writes;
    unsigned drop_write_every;  /* drop every Nth register write (0 = never) */
    unsigned marc_flicker;      /* next N MARCSTATE reads return changing junk */
    bool xfer_fails;

    /* Observations. */
    uint8_t write_addr[SIM_LOG];
    int writes;                 /* register writes that landed */
    uint8_t strobe[SIM_LOG];
    int strobes;
    int forbidden;              /* PATABLE/FIFO/test-register writes reaching the chip */
    int txns;

    /* Internal. */
    bool cs;
    bool so_high_latched;
    int byte_in_txn;
    uint8_t hdr;
    unsigned write_seq;
} cc1101_sim_t;

/* Power-on state: SWRS061I reset values for the registers the tests use. */
void cc1101_sim_init(cc1101_sim_t *sim);

/* A bus whose callbacks drive sim. */
cc1101_bus_t cc1101_sim_bus(cc1101_sim_t *sim);

#endif /* OSCILLA_CC1101_SIM_H */
