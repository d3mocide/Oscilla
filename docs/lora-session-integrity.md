# LoRa Session Integrity Tracker

**Status:** LSI-1 through LSI-4 hardware-confirmed; LSI-5's known-source
half hardware-confirmed, source-absent control still inconclusive; LSI-7's
`hw_fault` counter hardware-confirmed not to false-fire, genuine-fault
detection still unverified; LSI-8 (sync-word write, second profile, manual
entry) hardware-confirmed 2026-09-22. A follow-up review pass (register-write
audit, config-on-ack, DIO1 re-service) hardware-confirmed 2026-09-23 — see
below.  
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
| LSI-7 | Track mid-session SX1262 hardware faults, not just queue drops and CRC/header errors. Scoped to a concrete gap found in `lora_radio.c`'s task loop: a `GetIrqStatus`/`ClearIrqStatus` SPI transaction failing while already listening was previously silent — no counter, no log. | New `hw_fault` counter, wired through `lora_status`, the deck model, the Sub-GHz view, and the health CSV sidecar. Host-tested (partial-reply rejection included). | Hardware-confirmed 2026-09-23 that the counter (and the DIO1 re-service logic added alongside it, see below) stays at 0 under real MeshCore traffic — no false-fire. Genuinely forcing a fault (bus glitch / dropped `[CFG]` reply) to confirm it *fires* is still untested; needs fault injection, not a normal bench pass. |
| LSI-8 | Manual/multi-profile `lora_config`: a real LoRa sync-word register write (closing a driver gap — see below), a second named profile (Meshtastic US LongFast), on-device profile cycling, and arbitrary manual entry via the debug console. | `lora_config` accepts an optional 5th `sync_word` arg (defaults to `0x12`, no behavior change for existing 4-arg callers); `x` cycles `model::kLoraProfiles` on the Sub-GHz card; `lora_manual <freq> <sf> <bw> <cr> [sync]` debug command for arbitrary values; sync word recorded in the session manifest. Host-tested (profile table values pinned against LoRaTrace-RX's sourcing). | Sync-word write and MeshCore no-regression hardware-confirmed 2026-09-22. On-device `x` profile-cycling: the underlying cycle worked all along, but `subghz_view.cpp` only ever drew the pending-profile label in the `!hasConfig()` state, so it looked broken once a config existed — found and fixed 2026-09-23 (flashed, not yet operator-confirmed on the physical screen). Real Meshtastic reception still untested (needs a real nearby node). |

## Session contract

The C5 is the authority for radio-path counters. The deck records its own SD
write/drop counters separately. A health row therefore distinguishes:

| Signal | Meaning | Does not prove |
|---|---|---|
| `rx` | SX1262 raised a received-packet event. | A packet was decoded or persisted. |
| `crc_err` / `header_err` | The radio reported that receive failure. | Total on-air traffic or interference level. |
| `irq_drop` / `radio_drop` | Firmware could not enqueue an IRQ or radio event. | An RF-layer loss estimate beyond that queue boundary. |
| `ocp_drop` | The probe could not enqueue a LoRa record for OCP delivery. | A deck or SD write failure. |
| `hw_fault` | A `GetIrqStatus`/`ClearIrqStatus` SPI transaction to the SX1262 failed, or the IRQ status read back empty while DIO1 was asserted, during a running session. The radio task re-services a still-asserted DIO1 instead of stalling, so a persistent fault keeps this climbing rather than freezing at 1. | The chip is unresponsive going forward — a single bus glitch and a wedged chip both increment this the same way; only its rate of growth tells them apart. |
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
Every confirmed MeshCore reception to date rode on the register's reset
value, `0x1424` — which is exactly sync word `0x12` (the private-network
word) nibble-packed with control bits `0x44`. So MeshCore worked by design,
not luck; Oscilla just never wrote or verified it. (Reset value per the
SX126x register table, which this repo doesn't archive locally; it matches
RadioLib's private-network encoding, and the no-regression run below is
consistent with it.)
`lora_set_sync_word()` (new, in `lora_radio.c`) now writes it explicitly —
opcode `0x0D` and register `0x0740`/`0x0741`, cross-checked against
RadioLib's `SX126x::setSyncWord`/`SX126x_registers.h` (a widely-deployed,
independently-verified SX126x driver), not a datasheet PDF this repo has a
local copy of. The default (omitted 5th `lora_config` arg) is `0x12` —
chosen specifically so it's a no-op for MeshCore and for every existing
4-arg caller.

**The one thing this must confirm on real hardware before anything else:**
that writing the sync-word register explicitly does not change MeshCore
reception at all — i.e., that the explicit write matches the reset value
the chip was already using. Re-run a
short known-source MeshCore session (same setup as the LSI-5 known-source
run) after flashing and confirm `rx`/packet counts look the same as before
this change landed. Only after that holds is a Meshtastic run (new profile,
untested on Oscilla's own hardware, needs a real nearby Meshtastic node)
meaningful evidence rather than noise.

**New provenance field:** the session manifest now records `sync_word` in
hex, alongside `profile`/`freq_hz`/`sf`/`bw_khz`/`cr`.

### Hardware-confirmed 2026-09-22

Both checks above ran, over the debug console (driven remotely, no physical
key presses — `lora_manual` bypasses the screen/cursor gating by design).

**MeshCore regression check (`log_0025`):** 93 packets over 635.5s, zero
`crc_err`/`hw_fault`/`irq_drop`/`radio_drop`, 2 `header_err`, largest
inter-packet gap 35.8s (well under the 60s threshold that mattered for the
original silent-stall bug). **No regression** — reception rate, timing, and
error counters are consistent with or better than the pre-LSI-8 baseline
(`log_0023`: 282 packets/2327.9s). One honest observation, not a concern:
mean RSSI was -69.0 dBm here vs -81.9 dBm in that baseline — a real shift,
but sync word only gates which packets pass the filter, it cannot change
the RSSI of a packet that does get through. A shorter 10-minute window
sampling a different mix of MeshCore's flooded retransmissions is a far
more likely explanation than a code defect, and `lora-harness.md` already
documents this kind of session-to-session RSSI variance as normal for this
network's flooding behavior.

**Meshtastic-parameter mechanical test (`log_0026`, via `lora_manual
906875000 11 250 1 43`):** manifest correctly recorded `profile=manual`,
`sync_word=0x2B` — the new field threads through end-to-end on real
hardware. Zero packets over 634.7s (no known Meshtastic source in range,
expected) but also zero errors/faults of any kind while the driver held a
sync word it had never written to real silicon before this session. This
is the first proof the `WriteRegister` path works for a value other than
the reset default, and — as a bonus — it's also the first zero-packet
session captured, satisfying the zero-packet SD case LSI-3/LSI-5's
acceptance plan never had a real example of: the manifest and health CSV
both opened, wrote monotonic zero rows for the full session, and closed
correctly with nothing to log.

**Still open:** an actual Meshtastic *reception* test needs a real nearby
Meshtastic node, which wasn't available for this session — today's
Meshtastic run proves the mechanism doesn't fault, not that real Meshtastic
traffic decodes correctly.

**No on-device numeric entry.** Arbitrary manual values still require the
debug console over USB (`lora_manual <freq_hz> <sf> <bw_khz> <cr>
[sync_word]`) — this keyboard has no numeric-entry widget, so "manual" in
the field, no laptop, is still not possible. Only the two named profiles
are cyclable on-device (`x` key, Sub-GHz card) — see the 2026-09-23 note
below on a display bug that made this look non-functional.

## Review fixes hardware-confirmed (2026-09-23): DIO1 stall, config-on-ack, busy-reject

Re-reviewing the LSI-7/LSI-8 commits (before today's session, see WORKLOG
14107f6/c9a4bed) found three defects, fixed and now bench-confirmed:

- **Receive-only audit gap.** LSI-8's `write_register()` could write any
  SX1262 register; `check_rx_only.py` never looked at it. Now allowlisted to
  `0x0740` only, with mutation tests proving any other address goes red.
- **Config provenance.** The deck committed a `lora_config` to its model on
  *send*, not on ack — a rejected `lora_manual` would show and record
  parameters the probe never applied. Now commits only on `[CFG]`.
- **DIO1 stall.** A failed `GetIrqStatus`/`ClearIrqStatus` left an IRQ bit
  set on the edge-triggered line — the 2026-09-13 stall class, reopened.
  The radio task now re-services a still-asserted DIO1 instead of stalling
  (this is what LSI-7's `hw_fault` semantics above already describe).

**Hardware-confirmed 2026-09-23**, driven remotely over the debug console
(same approach as LSI-8's): a 90 s MeshCore listen showed `rx` climbing
(3→6→7→11→12→12) with `hw_fault` staying `0` throughout — the DIO1
re-service introduced no regression or false fault. A rejected
`lora_manual` (bandwidth `63`, not a valid SX1262 value) drew `err
code=badarg` and left `dump`'s `lora_cfg` unchanged, confirming the
ack-gated commit on real hardware. A `lora_manual` sent while already
listening drew `err code=busy`, confirming the probe's busy-reject. Full
detail in WORKLOG 2026-09-23.

**Not exercised this pass:** `c9a4bed`'s two specific edge cases (a timed-out
`lora_config` leaving stale `lora_config_pending_`; DIO1's double-service
possibly double-counting an empty status read as `hw_fault`) have no
console-reachable trigger — they need a deliberately dropped `[CFG]` reply
or an injected bus glitch, neither attempted here. Host-suite-verified only.

**Also found and fixed 2026-09-23: SubGhz `x` display bug.** On-device
profile cycling (`x`, Sub-GHz card) turned out to work correctly underneath
— `lora_profile_index_` kept advancing — but `subghz_view.cpp` only ever
drew the pending-profile label in the `!hasConfig()` branch, so once a
config existed there was no on-screen feedback on what `x` had selected.
Fixed by showing `x=<LABEL>` next to the applied config whenever the
pending selection differs from it. Flashed; not yet operator-confirmed on
the physical screen (no debug-console path exercises the `x` key itself).

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
