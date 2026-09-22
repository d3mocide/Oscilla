# LSI-5 — LoRa receive-only soak procedure

Procedure, not a result. Tracked in
[`docs/lora-session-integrity.md`](../lora-session-integrity.md) as LSI-5.
Run this and record the outcome as a new dated section below (or a follow-up
doc if it grows) — do not overwrite this procedure with results.

**Purpose:** the session-integrity counters (`rx`, `crc_err`, `header_err`,
`irq_drop`, `radio_drop`, `ocp_drop`) are only meaningful evidence once they've
been read under a *controlled* condition — a source known to be transmitting,
and a session where none is expected. `lora-harness.md`'s ~2h soak already
proved why this matters: an uncontrolled quiet period and a real silent
firmware stall produce the same symptom (`rx` stops moving) unless something
external confirms which one happened.

## Before starting

- **Build identity.** LSI's own deferred-decisions list notes the manifest
  doesn't carry a build identifier yet. Until it does, record
  `git rev-parse --short HEAD` for both firmware trees by hand alongside the
  SD card pull for this run — the manifest's `profile`/`freq_hz`/`sf`/`bw_khz`/`cr`
  fields identify the radio config, not the firmware build.
- **Device/antenna setup.** Note antenna model/placement, bench vs. field,
  and anything from the bench-power rule (AGENTS.md gotcha 15 — both boards
  on their own USB, Grove 5V disconnected, unless this run is specifically
  testing Grove-powered operation).
- **Receive-only.** This procedure only ever configures and listens
  (`lora_config` / `lora_listen` / `lora_status` / `stop`). No verb here
  transmits, and none should be added to make a "known source" — the known
  source is an independent transmitter you don't control from Oscilla.

## Run 1 — known-source

1. Configure the already-qualified `meshcore_us_ca` profile (deck: `c` on the
   LoRa RX / Sub-GHz card) against the same nearby MeshCore repeater
   `lora-harness.md` confirmed traffic from.
2. Start the listener (`s`) and let it run for a fixed, decided-in-advance
   duration — 30–60 minutes is enough to clear the ~30 minute mark where the
   original silent-stall bug (`lora-harness.md`) surfaced.
3. Record, at session end: the final health row's counters, `packet_rows`
   from the `.health.csv` sidecar, and the packet CSV's row count.
4. **Cross-reference against an independent observer if one is available**
   (`lora-harness.md` documents the PyMC endpoint and its auth — same
   caution applies: never commit the API key or paste it into a doc). A
   healthy run's `rx` count should track the observer's rate; a quiet
   `rx` count that doesn't match a quiet observer means the receiver is
   the thing that's actually stopped, not the mesh.
5. **What would fail this run:** `rx` counter stops advancing while an
   independent observer keeps seeing traffic (the silent-stall class of
   bug); any nonzero `irq_drop`/`radio_drop`/`ocp_drop` sustained across the
   run rather than a one-off blip; a `.health.csv` gap longer than a couple
   of poll intervals (~20–30s) with the listener still shown active.

## Run 2 — source-absent control

1. Configure a **manual** profile (not `meshcore_us_ca`) at a frequency/SF/BW
   combination with no known active network — the point is to establish what
   the counters look like when genuinely nothing is expected, not to survey
   for one. Record the exact parameters used; per LSI-1's non-goals, a manual
   profile is not a protocol assertion or a receive qualification of that
   band.
2. Run the same fixed duration as Run 1.
3. **Expected result:** `rx` stays at or near 0, and `crc_err`/`header_err`
   likewise — a healthy passive receiver hearing nothing produces empty
   counters, not error counters. A run that logs steady `crc_err`/`header_err`
   growth with `rx=0` suggests noise-floor or configuration trouble, not
   silence, and is worth its own note before trusting Run 1's quiet periods
   as "no source" in the future.
4. Confirm the SD sidecars still open/close correctly for a session that logs
   zero packets — LSI-3's acceptance evidence doesn't have a zero-packet case
   yet, and this run provides one.

## Recording the outcome

Append a dated section below this line with: both runs' final counter deltas,
build identity (commit hashes), device/antenna setup, and (if used) the
observer cross-reference. No raw hex payloads, no RSSI/SNR value dumps —
aggregate counts and deltas only, per the project's field-data policy
(AGENTS.md §3.5, `SECURITY.md`).

---

## Run results — 2026-09-22

**Build identity:** `c2e572e` (both firmware trees; no `firmware-c5/` or
`firmware-cardputer/src/` changes since). **Setup:** bench power rule
followed (each board own USB, Grove 5V disconnected). Run 2 used the
antenna-disconnect substitution noted above, not a manual off-frequency
profile (that UI doesn't exist yet — see LSI-5 status). No independent
observer cross-reference used this round.

| | Run 1 — known-source (antenna on) | Run 2 — antenna disconnected |
|---|---|---|
| Session | `log_0023` | `log_0024` |
| Duration | 2327.9s (~38.8 min) | 2145.2s (~35.8 min) |
| `rx` (final) | 282 packets logged | 283 packets logged |
| `crc_err` | 1 | 0 |
| `header_err` | 3 | 7 |
| `irq_drop` / `radio_drop` / `ocp_drop` | 0 / 0 / 0 | 0 / 0 / 0 |
| Largest inter-packet gap | 57.9s (0 gaps > 60s) | 52.1s (0 gaps > 60s) |
| RSSI mean / median / range | -81.9 / -81 / -101 to -69 dBm | -110.3 / -110 / -115 to -106 dBm |

**Run 1 (known-source): clean pass.** No stall, no drops, shape matches the
already-confirmed-fixed soak in `lora-harness.md` (55.1s max gap there vs.
57.9s here). Health rows, manifest, and packet CSV all correct.

**Run 2 (source-absent control): did not achieve a genuine no-source
condition, and that is itself the finding.** RSSI dropped by a real ~28 dB
mean (-81.9 → -110.3 dBm) confirming the antenna-disconnect substitution
did meaningfully attenuate the signal — but the nearby MeshCore repeater is
strong enough at this bench location that even the attenuated signal still
decoded at essentially the same packet rate as Run 1 (283 vs. 282 packets).
The counters and sidecars behaved exactly as designed and faithfully
reported a real, if weak, received signal — this is not a bug in the
session-integrity code. It does mean:

- The zero-packet SD-sidecar case (recording step 4 above) is **still not
  demonstrated** — every session captured on this bench so far has logged
  real packets.
- A genuine source-absent control at this location needs either real
  physical distance/shielding from the repeater, or tuning off-frequency
  (blocked on the not-yet-built manual-profile UI, tracked as a deferred
  decision in `docs/lora-session-integrity.md`).
- Worth trying next, and simpler than relocating the bench: power down the
  known nearby repeater used for Run 1 and repeat Run 2 with the antenna
  reconnected. Removes the source instead of attenuating the receiver, which
  is the actual confound this attempt ran into. Caveat: MeshCore floods
  across every repeater in range, so this only reads as "silent" if that
  repeater is the sole nearby node — worth noting in the next write-up
  whether that's known to be true or just assumed.

LSI-5's known-source requirement is satisfied by Run 1. The source-absent
requirement remains open pending a run that actually achieves low signal.
