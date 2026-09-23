# LoRa Session Integrity Tracker

**Status:** LSI-1 through LSI-4 and LSI-7 hardware-confirmed; LSI-5's
known-source half hardware-confirmed, source-absent control still
inconclusive; LSI-8 (sync-word write, second profile, manual entry)
implemented and host-tested, hardware confirmation pending (2026-09-22)  
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
  Two named profiles exist: MeshCore USA/Canada (hardware-qualified, P3) and
  Meshtastic US LongFast (source-verified against LoRaTrace-RX's
  `channel_plans.h` and upstream firmware, **not yet receive-qualified on
  Oscilla's own hardware** — see LSI-8). Future profiles require the same:
  a real source, and a separate receive qualification before being trusted.
- Build/test results, SD-file inspection, and RF/soak evidence are separate
  gates. None substitutes for another.

## Workstreams

| ID | Work | Acceptance evidence | Status |
|---|---|---|---|
| LSI-1 | Name the current MeshCore USA/Canada configuration and preserve an explicit manual path. | Profile token and exact frequency/SF/BW/CR appear in the deck state and session manifest. | Hardware-confirmed 2026-09-22 |
| LSI-2 | Count radio-path health signals. | `lora_status` reports RX, CRC/header errors, DIO1-queue drops, radio-event drops, and OCP-event drops. Counters reset at a documented session boundary. | Hardware-confirmed 2026-09-22 |
| LSI-3 | Make SD sessions self-describing. | Each numbered packet CSV has a matching manifest and append-only periodic health CSV; write failures/drop counts are recorded. | Hardware-confirmed 2026-09-22 |
| LSI-4 | Surface the same state on the deck without claiming RF meaning from an empty counter. | The Sub-GHz view displays profile/configuration and the latest health snapshot. | Hardware-confirmed 2026-09-22 |
| LSI-5 | Establish a repeatable receive-only soak procedure. | Controlled known-source and source-absent runs, build identity, device/antenna setup, counter deltas, and manual log inspection in `docs/hardware/`. | Known-source run hardware-confirmed 2026-09-22 (clean, no stalls/drops); source-absent control run but did not reach a genuine no-signal condition (bench too close to a strong repeater) — see [`docs/hardware/lora-session-soak.md`](hardware/lora-session-soak.md) |
| LSI-6 | Complete combined power/SD/TFT qualification after the panel arrives. | P6 current/supply measurements and a multi-hour all-subsystem soak. | Blocked on P5 hardware |
| LSI-7 | Track mid-session SX1262 hardware faults, not just queue drops and CRC/header errors. Scoped to a concrete gap found in `lora_radio.c`'s task loop: a `GetIrqStatus`/`ClearIrqStatus` SPI transaction failing while already listening was previously silent — no counter, no log. | New `hw_fault` counter, wired through `lora_status`, the deck model, the Sub-GHz view, and the health CSV sidecar. Host-tested (partial-reply rejection included). | Implemented and host-tested 2026-09-22; hardware confirmation pending |
| LSI-8 | Manual/multi-profile `lora_config`: a real LoRa sync-word register write (closing a driver gap — see below), a second named profile (Meshtastic US LongFast), on-device profile cycling, and arbitrary manual entry via the debug console. | `lora_config` accepts an optional 5th `sync_word` arg (defaults to `0x12`, no behavior change for existing 4-arg callers); `x` cycles `model::kLoraProfiles` on the Sub-GHz card; `lora_manual <freq> <sf> <bw> <cr> [sync]` debug command for arbitrary values; sync word recorded in the session manifest. Host-tested (profile table values pinned against LoRaTrace-RX's sourcing). | Implemented and host-tested 2026-09-22; hardware confirmation pending (see caution below) |

## Session contract

The C5 is the authority for radio-path counters. The deck records its own SD
write/drop counters separately. A health row therefore distinguishes:

| Signal | Meaning | Does not prove |
|---|---|---|
| `rx` | SX1262 raised a received-packet event. | A packet was decoded or persisted. |
| `crc_err` / `header_err` | The radio reported that receive failure. | Total on-air traffic or interference level. |
| `irq_drop` / `radio_drop` | Firmware could not enqueue an IRQ or radio event. | An RF-layer loss estimate beyond that queue boundary. |
| `ocp_drop` | The probe could not enqueue a LoRa record for OCP delivery. | A deck or SD write failure. |
| `hw_fault` | A `GetIrqStatus`/`ClearIrqStatus` SPI transaction to the SX1262 failed while a session was already running. | The chip is unresponsive going forward — a single bus glitch and a wedged chip both increment this the same way. |
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

## LSI-8: sync-word register write, second profile, manual entry (2026-09-22)

Software slice only — nothing below is hardware-confirmed yet.

**What changed and why.** `lora_radio.c` never wrote the SX1262's LoRa sync
word register (no `WriteRegister` opcode existed in the driver at all,
found while investigating LoRaTrace-RX's preset table, which does set one).
Every confirmed MeshCore reception to date rode on whatever the chip's
actual reset-default sync word happens to be, not a verified `0x12` write.
`lora_set_sync_word()` (new, in `lora_radio.c`) now writes it explicitly —
opcode `0x0D` and register `0x0740`/`0x0741`, cross-checked against
RadioLib's `SX126x::setSyncWord`/`SX126x_registers.h` (a widely-deployed,
independently-verified SX126x driver), not a datasheet PDF this repo has a
local copy of. The default (omitted 5th `lora_config` arg) is `0x12` —
chosen specifically so it's a no-op for MeshCore and for every existing
4-arg caller.

**The one thing this must confirm on real hardware before anything else:**
that writing the sync-word register explicitly does not change MeshCore
reception at all — i.e., that `0x12` really is what the chip was already
defaulting to, not a lucky coincidence of a different mechanism. Re-run a
short known-source MeshCore session (same setup as the LSI-5 known-source
run) after flashing and confirm `rx`/packet counts look the same as before
this change landed. Only after that holds is a Meshtastic run (new profile,
untested on Oscilla's own hardware, needs a real nearby Meshtastic node)
meaningful evidence rather than noise.

**New provenance field:** the session manifest now records `sync_word` in
hex, alongside `profile`/`freq_hz`/`sf`/`bw_khz`/`cr`.

**No on-device numeric entry.** Arbitrary manual values still require the
debug console over USB (`lora_manual <freq_hz> <sf> <bw_khz> <cr>
[sync_word]`) — this keyboard has no numeric-entry widget, so "manual" in
the field, no laptop, is still not possible. Only the two named profiles
are cyclable on-device (`x` key, Sub-GHz card).

## Retention policy (decided 2026-09-22)

**No automatic deletion or rotation of session files, ever.** The operator
manages the SD card manually. Chosen deliberately over a count-based cap:
an automatic-delete policy risks silently discarding a real field
observation, which is a worse failure mode than an SD card slowly filling
up — consistent with `SECURITY.md`'s "never commit field data" caution
applied one step further (don't auto-*delete* it either). Revisit only if
the 9999-session index cap (`lora_logger.cpp`'s `kMaxSessionIndex`) ever
becomes a real constraint, which is nowhere close today.

## Build identifier — design only, not implemented (2026-09-22)

Confirmed the prerequisite this was blocked on doesn't exist yet: both
firmwares report a manually-bumped human version string
(`firmware-c5/CMakeLists.txt`'s `PROJECT_VER`, `firmware-cardputer`'s
`OSCILLA_DECK_VER`), deliberately not git-derived (the CMake comment says
so explicitly), and neither has moved since 2026-09-13 despite everything
built since — `ver=0.2.0` cannot distinguish which commit produced a given
SD session. The design below is written up for a future implementation
session, not built now.

- **Don't touch `PROJECT_VER`/`OSCILLA_DECK_VER`.** They're a deliberate,
  documented choice (human-readable semantic version, not build noise) —
  add a separate field alongside them, don't repurpose them.
- **C5 (ESP-IDF/CMake):** capture `git rev-parse --short=8 HEAD` (plus a
  dirty-tree check) in `firmware-c5/CMakeLists.txt` at configure time and
  expose it as a new compile definition, e.g. `OSCILLA_BUILD_ID`, to
  `main/`. Surface it as a new `[HELLO]`/`version` field (new `OCP_K_*`
  key) — additive to the wire protocol, not a replacement for `ver`.
- **Deck (PlatformIO):** `platformio.ini` already runs a pre-build hook
  (`extra_scripts = pre:cxx17.py`) — the same mechanism can inject a
  git-derived define the way `cxx17.py` already injects build flags,
  without a new pattern. The deck has no wire verb reporting its own
  version (it's a client, not a server — same reason `OSCILLA_DECK_VER`
  exists as a build flag instead of a verb reply); its build ID would show
  in the Info view's DECK section the same way.
- **Manifest:** record both independently — `build_id=<hash>[-dirty]`
  (probe) and `deck_build_id=<hash>[-dirty]` (deck) — since each firmware
  is flashed separately and can genuinely differ between sessions.
- **Open question for whoever implements this:** should a `-dirty` build
  (uncommitted local changes at build time) just get flagged in the
  manifest, or actively warn the operator that this session's evidence
  isn't reproducible from any commit? Leaning toward flag-and-warn, not
  block — a dirty bench build is normal during active development — but
  that's a real decision, not a default to assume silently.

## Counter-growth thresholds — reviewed, still correctly deferred (2026-09-22)

Revisited this alongside the other two deferred items. The gate stands:
LSI-5 has one clean ~39-minute known-source run and one inconclusive
source-absent control — not the controlled, long-duration, multi-condition
evidence base this needs. Setting numeric thresholds now would be guessing
exactly the way this project avoids. No change; still blocked on real soak
data (see `docs/hardware/lora-session-soak.md`'s open next-attempt note).

## Deferred decisions

- On-device numeric entry for truly manual (no-laptop) configuration —
  `lora_manual` over the debug console is the only path today (see LSI-8).
