# Changelog

One entry per version cut. A version is a milestone of what is built and
checked. It is **not** a claim that phase gates are met. Phase status lives in
[`ROADMAP.md`](ROADMAP.md), and the evidence lives in [`WORKLOG.md`](WORKLOG.md).
The process is in [`docs/RELEASING.md`](docs/RELEASING.md).

Both firmwares carry the version in the repo-root [`VERSION`](VERSION) file.
The OCP protocol version (`OCP_PROTO_VERSION` in `protocol/ocp.h`) is separate
and changes only when the wire contract does.

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
