# LoRa Session Integrity Tracker

**Status:** software slice implemented; hardware/SD evidence pending (2026-09-22)  
**Owner:** Oscilla P3 follow-up  
**Scope:** Wio-SX1262 receive-only capture through the C5, OCP, Cardputer, and SD card.

## Purpose

P3 proved that Oscilla can receive and render real MeshCore traffic. This
follow-up makes a recorded session interpretable: the saved data must say what
the receiver was configured to do, and it must expose loss or error signals
instead of silently presenting a short log as a quiet RF environment.

This tracker borrows the *evidence shape* from LoRaTrace-RX—configuration
provenance, periodic health, and bounded soak gates—not its radio driver,
discovery sweep, or any transmit-capable feature.

## Non-goals and boundaries

- Oscilla remains receive-only. No transmit, replay, jamming, CAD scan, or
  active probing is in scope.
- A named profile records radio parameters; it is not a protocol assertion or
  decoder selection. Frame labels remain structural best-effort guesses.
- This does not add a frequency sweep or a broad regional preset catalogue.
  The initial named configuration is the already hardware-qualified MeshCore
  USA/Canada preset. Future profiles require their source and a separate
  receive qualification.
- Build/test results, SD-file inspection, and RF/soak evidence are separate
  gates. None substitutes for another.

## Workstreams

| ID | Work | Acceptance evidence | Status |
|---|---|---|---|
| LSI-1 | Name the current MeshCore USA/Canada configuration and preserve an explicit manual path. | Profile token and exact frequency/SF/BW/CR appear in the deck state and session manifest. | Hardware-confirmed 2026-09-22 |
| LSI-2 | Count radio-path health signals. | `lora_status` reports RX, CRC/header errors, DIO1-queue drops, radio-event drops, and OCP-event drops. Counters reset at a documented session boundary. | Hardware-confirmed 2026-09-22 |
| LSI-3 | Make SD sessions self-describing. | Each numbered packet CSV has a matching manifest and append-only periodic health CSV; write failures/drop counts are recorded. | Hardware-confirmed 2026-09-22 |
| LSI-4 | Surface the same state on the deck without claiming RF meaning from an empty counter. | The Sub-GHz view displays profile/configuration and the latest health snapshot. | Hardware-confirmed 2026-09-22 |
| LSI-5 | Establish a repeatable receive-only soak procedure. | Controlled known-source and source-absent runs, build identity, device/antenna setup, counter deltas, and manual log inspection in `docs/hardware/`. | Procedure written 2026-09-22 ([`docs/hardware/lora-session-soak.md`](hardware/lora-session-soak.md)); runs pending |
| LSI-6 | Complete combined power/SD/TFT qualification after the panel arrives. | P6 current/supply measurements and a multi-hour all-subsystem soak. | Blocked on P5 hardware |
| LSI-7 | Track mid-session SX1262 hardware faults, not just queue drops and CRC/header errors. Today a hardware fault only surfaces as a one-shot `[ERR] hwfault` at `lora_listen`; nothing counts a fault that happens while already running. | TBD — needs a definition of what counts as a running-session fault on this radio and where in `lora_radio.c`'s task loop it would be observed, before any counter or wire field is added. | Planned (proposed 2026-09-22, scope not designed) |

## Session contract

The C5 is the authority for radio-path counters. The deck records its own SD
write/drop counters separately. A health row therefore distinguishes:

| Signal | Meaning | Does not prove |
|---|---|---|
| `rx` | SX1262 raised a received-packet event. | A packet was decoded or persisted. |
| `crc_err` / `header_err` | The radio reported that receive failure. | Total on-air traffic or interference level. |
| `irq_drop` / `radio_drop` | Firmware could not enqueue an IRQ or radio event. | An RF-layer loss estimate beyond that queue boundary. |
| `ocp_drop` | The probe could not enqueue a LoRa record for OCP delivery. | A deck or SD write failure. |
| deck SD counters | The deck could not persist a row or health record. | A C5 radio loss. |

All counters are monotonic within a listener session. Starting a listener is
the reset boundary; stopping it preserves the final values for status and
logging. OCP is still the wire authority: new field literals belong in
`protocol/ocp.h` and the protocol specification changes with it.

## Initial acceptance plan

1. Run host/protocol checks and both board builds; prove a deliberately broken
   parser/model test fails before accepting its green result.
2. With a known receiver configuration, start/stop/start and verify that C5
   counters reset only at the new listener session, while the deck creates a
   distinct numbered manifest/log set.
3. Inspect an SD card copy: manifest configuration must match the listener;
   health rows must be parseable and append-only; packet CSV must retain the
   existing schema.
4. Run a timed known-source test and a source-absent control, recording only
   aggregate counters, build identity, and bench setup. Do not commit raw
   field observations.
5. After the TFT is installed, repeat under concurrent TFT redraw and SD
   logging as part of P6—not before claiming combined-system reliability.

## Hardware confirmation (2026-09-22)

Both firmwares (probe `firmware-c5` esp32c5/uart, deck `firmware-cardputer`
cardputer-adv) flashed cleanly and booted without a crash-loop. On the bench:

- The deck's Sub-GHz view showed `MCORE` (not `MANUAL`) after `lora_config`,
  confirming the named `meshcore_us_ca` profile reaches the display, not just
  the model.
- Start / stop / start of the listener showed the health counters reset on
  the second start, confirmed both on-screen and in the SD evidence below —
  satisfying the LSI-2 session-boundary requirement.
- SD card inspection after the run: only the two sessions from this pass
  (`log_0021`, `log_0022`) carry `.meta`/`.health.csv` sidecars; the pre-feature
  sessions (`log_0001`-`log_0020`) are untouched, confirming the numbering scan
  doesn't retrofit or collide with old sessions. Each manifest's
  `profile=meshcore_us_ca`/`freq_hz`/`sf`/`bw_khz`/`cr` matched the configured
  listener. Health CSVs were append-only, headers matched
  `loraHealthHeader()`, and counters were monotonic within each session.
  Packet CSV header/column count were byte-identical to a pre-feature session
  — the sidecar addition did not touch the existing schema.
- Observed, not a defect: the final health row written at session-stop reuses
  the last *polled* status rather than issuing a fresh query, so
  `packet_rows` (deck-side) can be one ahead of that row's `rx` (C5-side) if a
  packet lands between the last 10s poll and the stop command. Both counters
  stay independently monotonic; this is the expected shape of two counters
  from different subsystems sampled at different times, per the session
  contract above, not a reconciliation guarantee between them.

Not yet run: a timed known-source/source-absent soak (LSI-5) and the
concurrent TFT+SD P6 qualification (LSI-6, blocked on hardware). No raw field
observations (hex payloads, RSSI/SNR values) are recorded here.

## Deferred decisions

- Profile selector UX and any additional regional/network profiles.
- Retention/rotation policy for session files.
- Thresholds that turn counter growth into an on-screen warning; establish
  them only after controlled and long-duration measurements.
- Whether manifest files should carry a firmware build identifier; that needs
  a stable shared source of build identity first.
