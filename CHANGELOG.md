# Changelog

One entry per version cut. A version is a milestone of what is built and
checked. It is **not** a claim that phase gates are met. Phase status lives in
[`ROADMAP.md`](ROADMAP.md), and the evidence lives in [`WORKLOG.md`](WORKLOG.md).
The process is in [`docs/RELEASING.md`](docs/RELEASING.md).

Both firmwares carry the version in the repo-root [`VERSION`](VERSION) file.
The OCP protocol version (`OCP_PROTO_VERSION` in `protocol/ocp.h`) is separate
and changes only when the wire contract does.

## v0.4.0 — 2026-11-26

**Phases:** P0–P5 exit gates met. P6, P7, P8 and P9 are in progress.
**OCP protocol:** 1 (unchanged; new verbs and the `rst` field are additive,
and the deck falls back on older probes). **Toolchain:** ESP-IDF 5.5.6,
PlatformIO Core 6.2.0, espressif32 7.0.1
([`docs/BUILD_REPRODUCIBILITY.md`](docs/BUILD_REPRODUCIBILITY.md)).

### Added
- **433 MHz OOK lane (D-20):** clean-room receive-only CC1101 driver on the
  shared sub-GHz SPI bus, FIFO capture without a GDO0 wire, Princeton and
  LaCrosse TX141THBv2 decoding, `ook_listen` / `ook_status` / `stop ook`
  concurrent with the PHY and LoRa lanes, and a deck OOK RX screen with an
  external view.
- **Probe-driven multi-band wardrive (`start_wardrive`):** Wi-Fi and BLE rows
  geotagged on receipt, WigleWifi-1.6 `Type=BLE` CSV, auto-resume, and the
  saved-session browser reads BLE rows.
- **Wi-Fi scope:** region setting `set_region` with default `US` (D-19), band
  and custom-channel filtering `set_band` / `set_channels`, and a D-UCB
  adaptive channel picker for sniffer, discovery and deauth (D-6).
- **Richer inspection:** RSN AKMs, channel width, Wi-Fi generation, Country
  IE and WPS in AP dossiers; multi-vendor passive BLE tracker classification
  (AirTag, FMDN, SmartTag, Tile, Chipolo); offline SD OUI and Bluetooth
  company-ID lookup.
- **Passive baseline and evil-twin watch** for Wi-Fi and 802.15.4 (WP-11).
- **SX1262 sub-GHz spectrum and waterfall sweep** (WP-8), band-aware image
  calibration and a cellular-band preset.
- **Signal Hunt:** single-target RSSI tracking with Geiger-style audio
  (WP-12b), plus acquire/loss cues, a waypoint chime and an original
  synthesized startup sound under the Audio Cues setting (muted by default).
- **Deck:** contextual key guide on `h` ([`docs/deck-controls.md`](docs/deck-controls.md)),
  GNSS fix card, idle display sleep, battery discharge trend, geotagged LoRa
  KML, session waypoints, LOGS sidecar inspection, and the overhauled
  7-row Settings inspector with SD LoRa profiles.
- **Probe:** CRC32-checked NVS config persistence (region, band, channels,
  LoRa RX), and the last reset reason as `rst=` in `[HELLO]` / `STATUS`.
- **Tooling:** `deck_stability.py` (combined deck + probe runs over the
  debug console), a bench soak for all three lanes, the `--synth-bench`
  probe build that injects made-up events to drive the deck past its table
  caps (nothing transmitted; the build script keeps it out of every other
  image), and `deck_synth_caps.py`.

### Changed
- **ESP-IDF 5.5.1 → 5.5.6.** The vendored IDFGH-17050 USB-flash patch is gone.
- **Probe memory:** queues, NimBLE pools, OCP block frames and the CC1101
  buffer moved to PSRAM, then the Wi-Fi driver buffers too (D-23). Internal
  low-water under a heavy sniffer run went from 8.5 KB to about 86 KB, at a
  cost of about 2% of unique networks in wardrive A/B runs. PSRAM stays at
  40 MHz; 80 MHz measured no gain.
- **Deck memory:** the external panel borrows live models instead of copying
  them per frame (D-21). The live Wi-Fi table caps at 256 rows and the BLE
  table at 96, replacing the longest-unseen row; the Wi-Fi baseline stores
  12 bytes per AP (D-22).
- Wardrive and LoRa session start walk the card once with `readdir` instead
  of probing names one by one.

### Fixed
- The USB bench transport silently dropped every reply over 1 KB.
- Wardrive resume and stream double-logging.
- False baseline authentication downgrades.
- Stale probe lanes on the deck after a link reconnect or an idle probe.
- An O(N²) SD session-numbering stall that timed out `lora_listen`.
- Idle sleep: a key pressed at the timeout deadline is no longer lost, and
  the wake chord is swallowed until it is released.

### Verified on hardware (since v0.3.0)
- P6 USB phases on the tested image: 7,200 s steady logging, 1,800 s scan
  switching and six use/idle cycles, all with zero link or SD faults.
- Two-hour combined deck + probe runs with D-21, then with D-22 + D-23
  (deck free low 70 KB, probe low 85 KB, zero link errors).
- Tables driven past both caps with synthetic events: the wardrive log kept
  writing (4,254 AP / 2,783 BLE rows) with zero rejected or dropped events.
- OOK lane against rtl_433 and a Flipper (made-up Princeton key) with all
  three lanes live.

### Known open
- P6: battery soak, measured current budget and supply dips, and the power
  interlock thresholds.
- P7: a real AirTag, a moving-tracker run, a moving field wardrive, a WiGLE
  import of the BLE CSV, the 5 GHz DFS bench check (D-14), OOK SD logging,
  JTAG attach on the C5 harness, and a deck hardware session on the OOK
  screen.
- P8: on-device review of the key guide, dossiers and session browser;
  physical-key test of the display-timeout patch; individual restore checks
  for band, channels and LoRa parameters from probe NVS.
- D-22: that rows still being heard keep their place once the tables are
  full is host-tested only; the deck has no readout for it.

## v0.3.0 — 2026-11-20

**Phases:** P0–P5 exit gates met. P6, P7 and P8 are in progress.
**OCP protocol:** 1 (unchanged). **Toolchain:** ESP-IDF 5.5.1, PlatformIO Core
6.2.0, espressif32 7.0.1 ([`docs/BUILD_REPRODUCIBILITY.md`](docs/BUILD_REPRODUCIBILITY.md)).

### Added
- **External display:** the GMT028-05 ST7789 240×320 panel (Rev D §5.4) on
  the shared SD/TFT SPI bus under one lock. Native driver, row-cache diffing,
  and expanded companion views for every card: Wi-Fi, BLE, Sniffer, Packet
  Monitor, 802.15.4, LoRa, Drive, GNSS, Deauth, Anti-Surveillance and System.
- **Observatory v2 UI** on the deck LCD and the external panel: shared chrome,
  Chakra Petch / IBM Plex Mono bitmap fonts (OFL-1.1), solid RGB565 theme
  tokens, and a shared 5/6-row list grid across the nine list views.
- **Detail dossiers** for APs, BLE devices, contacts and LoRa packets:
  identity-pinned inspection, cached client relationships and RSSI history.
- **Saved-session browser:** read-only SD review of wardrive and LoRa
  sessions, paged records, and saved packet hex.
- **Passive suite**, built since v0.2.0: BLE scan with Find My
  classification, receive-only 802.15.4 recon, sniffer, deauth detection,
  packet monitor, continuous AP discovery, wardrive CSV/KML with GNSS, and
  anti-surveillance follow detection.
- **Tooling:** `render_external_views.py` (renders the real view sources to
  PNG for the brand guide), `soak_report.py`, power meter / power sequence
  bench tools, plus SD, external TFT, frame-memory and deck-stack checks.

### Fixed
- Deck allocation aborts during mode switching: overlapping snapshot tables
  removed, inactive external model copies released, and the AP inventory
  now grows in small blocks. The twelve-slot mode-switching soak passed.
- SX1262 RX stall after about 30 minutes (`GetIrqStatus` off-by-one).
- C5 Wi-Fi/BLE going silent after 802.15.4 stops: guarded reboot workaround
  (D-18, upstream defect).
- Garbled middle-dot glyphs in deck list views.

### Changed
- The external panel is now a standard fitment, not optional. Second-panel
  detection is dropped because a write-only panel cannot be detected
  (DESIGN §7.4).
- The no-CS ILI9341 path is removed. The ST7789 is the only supported
  external display.
- Version is read by both builds from a single `VERSION` file.

### Known open
- P6: periodic-use run (last attempt lost to a host USB drop), the new-image
  two-hour soak, a battery soak and the measured current budget.
- P7: a real AirTag, a moving-tracker run, and a fixture with more than 256
  APs.
- P8: operator review of dossier error states and the session browser, idle
  power saving (display timeout), and a field session on battery.
- P5 cold-boot and soak evidence is operator-reported (repeated SD cold
  boots, ~10 h on USB power); no captured log.

## v0.2.0 — 2026-09-13 (untagged)

Set in `7b597a3` and never tagged. P0–P2 exits met: OCP v1 contract, link
bring-up, probe Wi-Fi scanning, and LoRa RX under way.
