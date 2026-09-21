# bench-zig-emitter — 802.15.4 test-traffic source

**This is not Oscilla.** It is a throwaway ESP-IDF app for a *second*,
separate C5 board, built only to give Oscilla's receive-only Mesh
(802.15.4) engine real inbound frames to receive on the bench — this repo
had none, so `zig_recon.c`'s ISR/parse/table path had never actually been
exercised by live traffic, only by idle dwell.

It transmits IEEE 802.15.4 MAC frames. That is deliberately the *one* place
in this repository where that happens — Oscilla's own firmware
(`firmware-c5/`, `firmware-cardputer/`) stays receive-only, enforced by
`tools/check_rx_only.py`, which does not scan this directory (see
`PROBE_DIRS`/`DECK_DIRS` in that script — this only builds if `idf.py build`
is run from inside `bench-zig-emitter/` directly). Never fold this code into
`firmware-c5/main/`.

## What it does

Every ~150 ms, on a channel that steps 11 → 26 and wraps (faster than
Oscilla's own 400 ms/channel hop, so it doesn't need to be time-synced to
be seen), it sends one minimal, standards-shaped 802.15.4 data frame:
16-bit destination PAN `0xBEEF` + broadcast short address `0xFFFF`,
16-bit source short address `0x1234` (PAN-ID compression, so one PAN
covers both). No payload, no ACK request. That's enough for
`zig_frame_parse()` to accept it and `zig_table_upsert()` to register a
PAN/node sighting — matching what a real, minimal 802.15.4 device's MHR
looks like, without claiming to be Zigbee or Thread (D-17: MAC-only,
`802154` label).

## Building and flashing

Standalone ESP-IDF project, independent of `firmware-c5/`'s build:

```sh
source ../tools/env.sh   # from bench-zig-emitter/, same toolchain as the probe
idf.py set-target esp32c5
idf.py -p /dev/serial/by-id/<the SECOND C5, never the probe or deck> flash monitor
```

Double-check the port before flashing — this must never land on the
Oscilla probe or the Cardputer deck.

## Safety / regulatory posture

- Bench-supervised sessions only. Don't leave it transmitting unattended.
- `esp_ieee802154_transmit(frame, /*cca=*/false)` — no clear-channel check,
  by design, for a deterministic test signal. Short range, low duty cycle
  (one ~11-byte frame roughly every 150 ms).
- Same posture as the prior external-positive-control test documented in
  WORKLOG 2026-09-16: a second, separate board, used briefly, restored or
  retired afterward — not a permanent fixture of the bench rig.
