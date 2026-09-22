# Oscilla — Roadmap

> Master phase tracker. Supersedes the DESIGN.md v0.1 milestone table.
> **How to read this:** each phase has an **entry gate** (what must be true to start), the **work**, and an **exit gate** (the demonstrable thing that says it's done). A phase is not "done" until its exit gate is met on hardware. Update **Status** and the date as phases move; log the detail in [`WORKLOG.md`](WORKLOG.md).

| Field | Value |
|---|---|
| **Current phase** | P4 complete → **P5 — External TFT** (🔴 blocked on panel/regulator); P7 passive-suite work continues in parallel |
| **Last updated** | 2026-09-22 |
| **Hardware authority** | [`Research/c5-backpack-design.md`](Research/c5-backpack-design.md) Rev D |
| **Design authority** | [`DESIGN.md`](DESIGN.md) v0.2 |

---

## Status board

| Phase | Name | Status | Exit gate met? |
|---|---|---|---|
| **P0** | Reconcile & scaffold | 🟢 Exit gate met | ✅ 2026-09-12 |
| **P1** | Prove the link | 🟢 Exit gate met | ✅ 2026-09-12 |
| **P2** | Probe sees Wi-Fi | 🟢 Exit gate met | ✅ 2026-09-12 |
| **P3** | LoRa (RX) | 🟢 Exit gate met | ✅ 2026-09-13 |
| **P4** | GNSS on the deck | 🟢 Exit gate met | ✅ 2026-09-16 |
| **P5** | External TFT | 🔴 Blocked — panel/regulator pending | — |
| **P6** | Combined soak & power | ⚪ Not started | — |
| **P7** | Passive suite completion | 🟡 In progress (out of sequence) | — |
| **P8** | Polish | 🟡 In progress — deck UI foundation | — |

Legend: ⚪ not started · 🟡 in progress · 🟢 exit gate met · 🔴 blocked

---

## P0 — Reconcile & scaffold

**Why first:** the two source documents described different machines; the protocol header is the one artifact both firmwares compile against, so it must exist and be agreed before either firmware starts.

**Entry gate:** repo exists (met).

**Work:**
- [x] Reconcile DESIGN.md against backpack Rev D → v0.2.
- [x] Create ROADMAP.md, WORKLOG.md, docs/DECISIONS.md.
- [x] Monorepo skeleton per DESIGN.md §10 (`protocol/`, `firmware-c5/`, `firmware-cardputer/`, `tools/`, `docs/hardware/`).
- [x] `protocol/ocp.h` v1 — 31 verbs, marker registry, proto version, caps, error codes, limits. Carries an `OCP_VERB_TABLE` X-macro so the probe's dispatch table and the tooling derive from one list, and a `#error` tripwire that refuses to build if a transmit flag is defined (D-8).
- [x] `protocol/OCP-SPEC.md` — human spec mirroring DESIGN.md §5, plus the three things §5 left open: compact vs. block frames, the escaping rule, and `[HELLO]` precedence over parser state.
- [x] `tools/ocp_repl.py` — serial client, `--replay` for canned streams, `--selftest` for the spec's conformance checklist. Parser lives in `tools/ocp.py`; `--selftest`/`--replay` need no pyserial and no hardware.
- [x] `NOTICE` — projectZero / risinek MIT attribution.
- [x] *(added)* `protocol/test_ocp_header.c` + `tools/check_protocol.sh` — the exit gate as a runnable script.
- [x] *(added)* `tools/fixtures/boot_and_scan.txt` — the canned adversarial session.
- [x] *(added)* `tools/hoststub/Arduino.h` — lets the deck's framework-agnostic layers compile and be tested off-target (DESIGN §7.1).

**Exit gate:** `ocp.h` compiles standalone; `ocp_repl.py` runs and can frame/deframe against a canned fixture; both firmware directories build an empty app that `#include "ocp.h"`.

**Exit gate: MET on 2026-09-12.**

| Criterion | Result |
|---|---|
| `ocp.h` compiles standalone | ✅ `-Wall -Wextra -pedantic -Werror` clean under gcc c99/c11/c17 and g++ c++11/c++17; invariants asserted by `test_ocp_header.c` |
| `ocp_repl.py` frames/deframes a canned fixture | ✅ 21/21 conformance checks; fixture replays in 3- and 7-byte chunks |
| Both firmware dirs build an empty app | ✅ `oscilla-c5.bin` for **esp32c5** (ESP-IDF v5.5.1, 209 KB, 80% of app partition free); `firmware.bin` for **esp32s3** (PlatformIO, RAM 5.6%, flash 7.9%) |

Reproduce with two scripts — `tools/check_protocol.sh` (host, no toolchain) and
`tools/build_firmware.sh` (both boards). Toolchain paths live in
`tools/env.sh`; nothing needs installing on the dev host.

**What the board builds did *not* settle:** ADV hardware support. That was D-12, resolved on hardware 2026-09-12.

---

## P1 — Prove the link

**Why:** the Grove UART with unavoidable C5 boot noise (Rev D §3, DESIGN §4.2) is the riskiest interface in the system. Prove it before building anything on top.

**Entry gate:** P0 exit met. Both boards on their own USB, Grove **5V (red) disconnected** (Rev D §9). Cardputer ADV board support confirmed ([D-12](docs/DECISIONS.md) ✅ 2026-09-12).

**Work:**
- [x] C5: boot → OCP server on the Grove UART (GPIO11 TX / GPIO12 RX, 115200 8N1). *Plus a USB Serial/JTAG bench transport (`--bench`) for driving the probe with no Grove cable.*
- [x] C5: implement `hello`, `ping`, `version`, `status`, `stop`, `reboot`. Emit unsolicited `[HELLO]` on boot.
- [x] *(added)* `protocol/ocp_text.{h,c}` — shared field escaping, cross-checked against `ocp.py` on 784 payloads.
- [x] *(added)* `ocp_repl.py --gate` — the exit-gate demo as assertions against a live probe.
- [x] Deck: transport layer — `src/ocp/ocp_parser` (byte-exact, bounded, never throws), diffed item-for-item against `ocp.py` on the fuzz corpus.
- [x] Deck: connection state machine `Disconnected → HelloSent → Ready(caps)` (+ `Incompatible`) — `src/ocp/ocp_client`, 33 host tests incl. reset mid-frame, timeouts, millis wrap, contract-only verbs.
- [x] *(added)* Deck app: link-status view, auto-reconnect, 3 s keepalive ping, key commands.
- [x] `tools/ocp_fuzz.py` — property fuzzer (no raise, chunk-invariant, noise-safe, recovery, bounded). Found three reference-parser bugs, all fixed and regression-guarded. `--emit-corpus` feeds the deck parser's diff test. Deck parser diffed against it (`tools/check_deck_parser.py`).

**Exit gate (two demos):**
1. `ocp_repl.py` on a laptop drives the C5 through the full system-verb set — *before the Cardputer firmware exists*. **✅ Met 2026-09-12 over the real Grove UART** through the Cardputer's `grove-bridge` firmware: `--gate` 18/18, four consecutive runs, `link=uart0`, with ROM boot text on the wire parsed as noise. Also met over USB. Record: [`docs/hardware/link-bringup.md`](docs/hardware/link-bringup.md).
2. With the deck connected, physically resetting the C5 mid-session produces a clean `[HELLO]`, the deck returns to `Ready`, and no boot text is ever mistaken for a frame. **✅ Met 2026-09-12 on hardware:** three RESET presses on the XIAO → three detected resets, deck stayed `ready`, **8 boot-noise lines per reset** (matches the captured ROM text), **`stray=0`**, 51 keepalive pongs with 0 timeouts and 0 errors across the session.

---

## P2 — Probe sees Wi-Fi

**Entry gate:** P1 exit met.

**Work:**
- [x] `radio_arbiter.c` — PHY-lane single-owner mutex + teardown hooks. *Power interlock stays a stub until P6 (no LoRa lane yet).*
- [x] `wifi_recon.c` / `wifi_inspect.c` — **passive** `scan_networks` ✅, `show_scan_results` with paging ✅, passive `inspect_network <i>` (MFP + uptime) ✅.
- [x] Frame emission: `[SCAN]` CSV rows, `[INSPECT]`.
- [x] *(added)* `beacon_parse.c` — fuzzed under ASan/UBSan; `tools/check_rx_only.py` — transmit-API denylist; `ocp_repl.py --gate-wifi` — 41 live checks + 1 skip (no WPA3-only AP in range).
- [x] Deck: **Wi-Fi Scan** + **AP Detail** views over real frames — `app/deck_app`, `model/scan_model`, `ui/sweep_view`, `ui/trace_view`; auto-paging; 16 KB drained RX buffer.

**Exit gate:** `scan_networks` and `inspect_network` verified via `ocp_repl.py` against a live AP, then the same data rendered in Wi-Fi Scan/AP Detail on the deck. `stop` always returns to idle.

**✅ Met 2026-09-12.** `ocp_repl.py --gate-wifi`: 41 passed, 1 skipped (no WPA3-only AP in range). On the deck over Grove, Will ran the walkthrough: scans of 134 and 125 APs arrived **with 0 malformed rows**, 4 inspects rendered MFP/uptime/interval (one AP with RSN and MFP off, one MFP-capable), and `stop` mid-scan aborted within ~0.2 s twice, with the next scan working normally.

---

## P3 — LoRa (RX)

**Why here:** it's the second-riskiest bring-up (Rev D §4.3 — BUSY, RF_SW/DIO2 coherence, TCXO). Receive-only per [D-8](docs/DECISIONS.md) — no transmit. [D-10](docs/DECISIONS.md) (TCXO startup delay/voltage) is resolved — `tcxoVoltage=0x02`, `delay=640` (10 ms) as a bench-verified starting point; RX params otherwise self-chosen.

**Entry gate:** P2 exit met. ✅ Wio harness assembled and wired point-to-point per Rev D §4 — verified electrically by a live, fault-free SPI bring-up sequence on 2026-09-13 rather than a static continuity check (stronger evidence either way). **Note:** wired without the NSS/RST/RF_SW pull resistors Rev D calls for — measured absent on the board, wired anyway as a deliberate bench-only call (see WORKLOG); still worth adding before calling this field-ready. ✅ Antenna attached.

**Work:**
- [x] `lora_radio.c` (RX path) — reset sequence, bounded BUSY waits (fault, never hang), DIO1 ISR → radio task, RF_SW/DIO2 set coherently for **receive**, TCXO via DIO3 with documented delay, DC-DC mode, SPI ~1 MHz then raise. **Bench-validated 2026-09-13**: full reset/TCXO/calibrate/RX-entry sequence ran fault-free against the real chip.
- [x] `lora_recon.c` RX — `lora_config`, `lora_listen`, `lora_status`; stream `[EVT] kind=lora`. Live-tested over the bench USB transport: config accepted, RX started and stopped cleanly. **Framing classification (meshtastic/lorawan/unknown) not yet written** — no real packet has been received yet to classify.
- [x] Deck: **LoRa RX** view. Written, built clean, flashed to hardware 2026-09-13, and **confirmed live**: real MeshCore packets observed scrolling through the actual Cardputer UI over Grove.
- [x] Build advertises `lora_rx` (confirmed live: `hello` → `caps=wifi24,wifi5,lora_rx`) and `check_rx_only.py` confirms no transmit-capable API anywhere in the source tree.

**Exit gate: MET on 2026-09-13.** Real sub-GHz packets (a nearby MeshCore repeater, USA/Canada preset: 910.525MHz/SF7/BW62.5/CR4:5, confirmed from MeshCore's own docs) observed with RSSI/SNR live in the Cardputer's LoRa RX view over Grove; `stop` releases the LoRa lane cleanly, including mid-traffic; `check_rx_only.py` confirms zero TX verbs anywhere in the source tree.

Still open, not blocking the gate: the NSS/RST/RF_SW pull resistors Rev D calls for are still not installed (measured absent, wired anyway as a deliberate bench call — see WORKLOG). `GetDeviceErrors`/`XOSC_START_ERR` was closed 2026-09-14 (see WORKLOG) — this line was stale and is corrected here.

Framing classification exists now: `model::classifyLoraFrame` (2026-09-14, extended same day) is a structural best-effort guess covering **meshtastic/lorawan/meshcore/unknown** — one more than DESIGN §9.1's original `meshtastic|lorawan|unknown` set, added because MeshCore is the only traffic actually seen on the bench so far (see P3 exit gate above). Meshtastic's fixed-size header + broadcast marker, LoRaWAN's MHDR + length invariants, and MeshCore's `Packet` header + path/payload length-consistency chain are all cited against the real upstream source (`meshcore-dev/MeshCore`'s `src/Packet.{h,cpp}`, fetched 2026-09-14) or spec, not guessed. 46 host tests, including several that caught real cross-talk between the LoRaWAN and MeshCore gates on the same synthetic fixture and had to be adjusted so each test isolates what it means to. **✅ Hardware-confirmed the same day.** Flashed to the probe, real MeshCore repeater traffic observed live in the Cardputer's Sub-GHz view: the `C` tag (green) rendered correctly next to real received packets — the classifier's structural guess actually fires on live traffic, not just the synthetic fixtures it was built against. Doesn't retire the classifier's own documented limits (the ~2.3% LoRaWAN-gate false-positive rate on non-LoRaWAN bytes is still arithmetic, not something one bench session rules out over a large enough sample), but the headline risk — real MeshCore traffic reading as `unknown` or misclassifying as `lorawan` — did not happen in this observation.

🟢 **Found, root-caused, and fixed, same day:** a 2h soak cross-referenced against an independent MeshCore observer showed real RX silently stalling after ~30 minutes with zero errors logged — a `GetIrqStatus` off-by-one that could leave a fired IRQ bit uncleared, and since DIO1 is edge-triggered, stuck-high forever. Fixed and bench-confirmed: a 66.8-minute retest logged 524 packets with the largest gap between any two consecutive packets at 55.1s (zero gaps over 60s, anywhere). See `docs/hardware/lora-harness.md` and WORKLOG 2026-09-13 for both the finding and the confirmation.

---

## P4 — GNSS on the deck

**Entry gate:** P2 exit met (independent of P3; can run in parallel).

**Work:**
- [x] Deck GNSS UART (GPIO13 TX / GPIO15 RX, 9600 8N1 NMEA), distinct hardware UART from Grove. Wired in `main.cpp` as `Serial2`; builds clean for `cardputer-adv`. **Confirmed on hardware 2026-09-16** — the ATGM336H's default 9600 baud was correct as shipped, no runtime negotiation needed.
- [x] NMEA parse; fix validity + age; **no-fix ≠ no-UART-data** as distinct states. `src/gnss/nmea_parser.{h,cpp}` (checksum-verified, bounded, chunk-invariant, byte-exact — same posture as `ocp::Parser`) feeding `src/model/gnss_model.{h,cpp}` (GGA/RMC → fix, `hasUartData()` vs `hasFix()`/`everFixed()`/`fixAgeMs()` kept independent). 40 host tests (`nmea_parser_test`, `gnss_model_test`), wired into `check_protocol.sh`. Confirmed a check actually catches a bug: disabling the checksum comparison on a copy flips the corrupted-checksum test red.
- [x] Configurable baud (a preconfigured unit may differ, Rev D §6). No UBX assumptions. Default `9600` in `main.cpp` as a single named constant (`kGnssBaudDefault`) — confirmed correct against the real unit 2026-09-16 (see above); no settings UI to change it at runtime, deliberately deferred (see below).
- [x] Model: current-fix service feeding the logger. `GnssModel` exists with the exact fields DESIGN §9.1's struct calls for (`lat/lon/alt/hdop/utc/valid` + derived `age_ms`), plus a full calendar date (`year/month/day`, parsed from RMC — GGA carries none) added 2026-09-14 for the wardrive CSV's FirstSeen column.
- [x] **Drive view + wardrive logging wired (2026-09-15), hardware-confirmed (2026-09-16).** `GnssModel` moved into `DeckApp` and now feeds `src/ui/gnss_view` — a home card showing fix state, position, HDOP, fix age, UTC date and the session's counters. The model's `GnssState` (`NoUartData`/`Searching`/`FixLost`/`Fixed`) means "no fix", "lost the fix" and "the receiver stopped talking" are one decision made once. `src/storage/wardrive_logger` opens a numbered session writing **both** a WigleWifi-1.6 CSV and a KML track+placemarks; `l` on the Drive card toggles it (also reachable via the debug console's `wardrive` command). 49 host tests on the model, including the antenna-unplug transition and a proof the checks go red when the states are collapsed — all since matched by the real transition on hardware.
- [x] **GNSS wire-corruption diagnostics (2026-09-16).** A card review found sessions producing far fewer distinct fixes than a steady lock should. Root-caused to `Serial2`'s RX buffer sitting at the Arduino core's 256 B default against a shared-SPI-bus SD flush on every AP row (Rev D §5.2) — fixed with `setRxBufferSize(2048)`, a real `onReceiveError()` hardware overflow callback, and checksum/overlong drop counters in `NmeaParser`. Confirmed fixed on a 21-minute outdoor session: 5254/5254 AP rows had a fix (zero `nofix`), 252/252 track points landed (vs. 3 before the fix), 251 of them distinct positions.
- [ ] Runtime baud setting. Still a compile-time constant (`kGnssBaudDefault` in `main.cpp`). The bench confirmed the delivered unit is 9600 as shipped, so a settings UI for a value that's never needed changing stays speculative work — revisit only if a different unit ever requires it.

**Exit gate: MET — 2026-09-16.** All three requirements demonstrated on real hardware in one session: a live fix acquired outdoors and held continuously for 21+ minutes with age shown (`age_ms` fresh every ~5s, zero drops); unplugging the antenna produced `fix lost` — not `no data` — while NMEA sentences kept arriving over UART (`chkfail`/`overlong` stayed at 0 through the drop, confirming the link itself never broke); reconnecting recovered cleanly back to `fix`. The wardrive session logged through the same window confirms the CSV/KML side of the gate too: every fixed observation was written, an earlier all-no-fix session (`drive_0003`) confirmed the `nofix` counter and empty CSV path both work when there's never a fix at all. Full detail in WORKLOG 2026-09-16.

**Follow-up 2026-09-17:** an indoor, poorly placed receiver remained in `fix lost`; the cumulative `chkfail=63` and `overlong=0` counters stayed flat for a 90-second read-only capture, with no new UART error. This is not a reproduced parser fault. A fresh outdoor, undisturbed receiver run remains the appropriate follow-up to the earlier connector disturbance.

---

## P5 — External TFT

**Why late:** waiting on the panel to arrive; and the shared SD/TFT SPI bus (Rev D §5.2) is the deck's hardest correctness problem.

**Entry gate:** panel in hand. P2 exit met.

**Work:**
- [ ] ILI9341 write-only init on the shared bus (CS1 5, DC 4, RES 6), MISO unconnected, ~4 MHz.
- [ ] **Single shared-bus lock** covering SD + TFT; SD brought up first; no task bypasses it.
- [ ] Second-panel detection at boot; expanded views on the 240×320.
- [ ] Touch stays disabled (Rev D baseline).

**Exit gate:** sustained TFT redraw concurrent with SD read/write of a disposable file, cold-boot with SD inserted, no bus corruption over a soak.

---

## P6 — Combined soak & power

**Why gating:** DESIGN §6.2's power interlock and any talk of Grove-powered/battery operation depend on **measured** current, not the provisional numbers in Rev D §9.

**Entry gate:** P2–P5 exits met.

**Work:**
- [ ] All subsystems active together: UI, SD, GNSS, Grove link, survey radios, LoRa.
- [ ] Measure current at module pins under simultaneous activity (Rev D §9 budget table).
- [ ] Record resets, timeouts, serial overruns, storage errors; measure supply dips.
- [ ] Set the real power-interlock thresholds in the arbiter from measured headroom.
- [ ] Cold-start / independent-reset recovery; native-USB flashing with the backpack attached.

**Exit gate:** a documented current budget in `docs/hardware/`; the arbiter's interlock reflects it; a clean multi-hour soak. Only after this does Grove-powered operation ([D-4](docs/DECISIONS.md)) get evaluated.

---

## P7 — Passive suite completion

**Entry gate:** P2 + P6 exits met.

P7 work is being exercised out of sequence while the external TFT and measured
power gates are pending. The items below distinguish implementation and
receive-only RF evidence from the final phase exit gate; P7 is not declared
complete until its remaining real-frame and view checks are demonstrated.

**Work:**
- [x] **BLE (2026-09-16).** `ble_recon.c` — NimBLE passive scan (`scan_bt`/`scan_airtag`, OCP-SPEC §11), device table, Find My/AirTag tracker classification (Apple company ID `004c` + type byte `0x12`, confirmed against public Find My protocol write-ups). Split probe-side into `ble_adv_parse.c` (AD-structure parsing) and `ble_device_table.c` (upsert logic), both pure C and host-tested (13 + 14 checks, plus 200k fuzz iterations under ASan/UBSan) — same shape as `beacon_parse.c`/`sniff_track.c`. Deck-side `src/model/bt_model` + **BLE Scan** view (`src/ui/bt_view`), 16 host tests. Enabling NimBLE overflowed the default 1 MB app partition by ~126 KB; fixed with Espressif's own "large single-app" table (1500 KB) rather than trimming anything — 8 MB of physical flash made that the easy call. Both firmwares build clean on the real toolchain (probe now at 76% of its larger partition). **Hardware-confirmed same day.** `scan_bt`/`scan_airtag` both verified against real BLE traffic (70–75 real devices per scan, 0 malformed rows) after fixing two real bugs a client-timeout mismatch (`scan_bt`'s reply arrived after the deck's generic 2s timeout had already given up — added `kBleScanTimeoutMs`) and a NimBLE semantics bug (`ble_gap_disc_cancel()` doesn't emit a completion event the way `esp_wifi_scan_stop()` does, so cancelling `scan_airtag`'s unbounded scan left the radio arbiter stuck forever — fixed with an idempotent `finish_scan()` called from both the natural-completion and forced-cancel paths). Full detail in WORKLOG. **Still open:** the BLE Scan card hasn't been looked at on the deck's actual display yet (same gap Packet Monitor has); no real AirTag on hand to confirm the tracker classification against genuine hardware. The in-house 802.15.4 recon slice is now planned under D-17.
- [x] **802.15.4 passive recon (in-house, 2026-09-16).** Oscilla-native,
  receive-only `zig_radio.c` (bounded lifecycle/channel dwell), `zig_frame.c`
  (hostile-frame-safe MAC parsing and MAC-only `802154` label), `zig_table.c`
  (capped PAN/node tracking), and `zig_recon.c` (arbiter/stop and `[ZIG]`),
  plus the **802.15.4** view. No association, commissioning, network-layer decode,
  keys, or transmit path. **Hardware-confirmed:** a separate C5 at -80 dBm
  yielded live PAN `1a2b` / node `1234` with zero drops through Grove/OCP;
  a known-address external positive control returned a 5-byte ACK, while the
  identical ACK-requested unicast to the temporary fixed-identity, promiscuous
  Oscilla image returned ESP-IDF `NO_ACK` and was independently observed and
  captured by Oscilla. Normal images restored. Remaining: physical 802.15.4-view
  inspection and long-duration/loss characterization. **2026-09-21:** found and
  worked around an ESP32-C5 coexistence defect where stopping this engine left
  Wi-Fi/BLE silently returning zero results until a full reboot — no app-level
  fix exists (upstream, [D-18](docs/DECISIONS.md)); the probe now auto-recovers
  with a guarded reboot, hardware-confirmed.
- [x] `start_sniffer`/`show_clients`/`show_probes` + **Sniffer** view — started early, out of sequence (see WORKLOG 2026-09-12). **Hardware-confirmed 2026-09-16**: `sniff` via the debug console picked up real clients/probes over live RF. 5 GHz hop set excludes DFS channels ([D-14](docs/DECISIONS.md): leaning — regulatory question closed 2026-09-14, one bench test left before flipping it).
- [x] `deauth_detector` + **Deauth Detect** Analyze card. **Hardware-confirmed 2026-09-16**: ran clean over real RF (0 events — no attacks present, which is the correct/expected result, not an untested path).
- [x] `channel_view`, `packet_monitor` + **Packet Monitor** view — same early/out-of-sequence batch. **Hardware-confirmed 2026-09-16**: `spectrum` and `channel <n>` both ack'd by the real probe over the debug console (9 real readings, `cfg ack ch=6`). Still not visually confirmed rendering correctly on the deck's own TFT — that check is cheap and worth doing next time the deck's in hand.
- [x] **Wi-Fi continuous AP discovery (2026-09-16/17).** `start_wifi_scan` passively hops the shared channel list, parses beacon/probe-response frames, deduplicates BSSIDs in a bounded table, and streams first-sighting `[EVT] kind=network` rows into the Wi-Fi Scan model (`c` key / `wifiscan` debug command). Host/protocol and both firmware builds pass. Live RF discovery, stop/restart, and a no-reset 10-minute soak are hardware-confirmed; the best uncontrolled baseline retained 216 rows with stable current heap and no run exceeded the 256-row cap. A controlled authorized fixture with more than 256 passive APs and final view inspection remain open.
- [x] Wardrive: stream observations, deck-side geotag against local fix + age, write WigleWifi CSV + KML; **Drive** view. `storage::wardriveCsv*`/`storage::kml*` format writers, `storage::wardrive_logger` for SD I/O, wired to `ScanModel`+`GnssModel`. **Fully hardware-confirmed 2026-09-16**: a 21-minute outdoor session produced `aps=5254 nofix=0 trk=252`, every count matching the device's own — see WORKLOG for the full antenna-unplug/GNSS-diagnostics story this closed out alongside P4.
- [x] **Anti-surveillance software slice (2026-09-17/18).** `start_antisurveillance`
  now runs the same passive Find My/AirTag classifier on the probe and streams
  `kind=airtag` sightings; the deck's dedicated **Anti-Surveillance** Analyze card
  correlates repeated sightings with fresh GNSS fixes in bounded RAM. It requires
  two separated movement legs of at least 25 m with a fix no older than 10 s before
  showing a conservative `FOLLOW?` candidate. Host/protocol tests and both
  firmware builds pass. Live tracker-classified traffic and the start/stop
  lifecycle were smoke-tested on the deck without the prior stuck-start state.
  **Still open:** a controlled moving-tracker run with two qualifying movement
  legs, final physical view inspection, and the external TFT rendering once P5
  is available.

- [ ] **CC1101 legacy sub-GHz (started 2026-09-21, D-15).** Second, receive-only
  sub-GHz peripheral alongside the Wio-SX1262, arbitrated by a new
  `subghz_arbiter.c` (exactly one active engine between the two, independent
  of the PHY lane) — LoRa's own lane now goes through the same arbiter
  instead of its prior ad-hoc bypass. New verbs `legacy_config`/
  `legacy_listen`/`legacy_status`, cap `legacy_rx`, `stop legacy` lane.
  **Hardware-confirmed:** `PARTNUM=0 VERSION=20` (matches the documented
  genuine-CC1101 signature) and fault-free RX start, both on real silicon;
  the arbiter correctly refuses either radio while the other is active, in
  both directions, without disturbing the running one (real hardware, both
  directions). A real SPI-timing bug (SO checked before CS was asserted)
  was found and fixed on the bench, not in review. Deck side: **CC1101 RX**
  card mirroring the LoRa screen (chunk list — "chunk," not "packet": no
  sync word, no CRC, so it's an unframed FIFO slice, not a decoded packet
  — plus a per-session SD log), added same night.

  **2026-09-22, Codex's independent review** (`Research/cc1101-passive-recon-review.md`)
  plus an 8-agent `/code-review` pass, cross-referenced, found the register
  math was wrong (DRATE ~1.587k not ~2.4k baud; channel filter 812.5 kHz,
  4x wider than intended), two real lifecycle bugs (LoRa leaked the
  sub-GHz arbiter owner on a rare task-creation failure; `legacy_status`
  could touch the shared bus without owning it), and a doc/code mismatch
  in the arbiter's documented behavior. All fixed same day — new
  host-tested `cc1101_regs.c` register-math module (with a mutation-style
  proof against the actual old wrong values), stable `RXBYTES` sampling,
  overflow/drop counters, bounded drain-task lifecycle waits. Full detail
  in WORKLOG.

  **Deferred, not dropped:** Codex's Workstream B (RSSI/carrier-sense
  activity-survey mode as a further evolution beyond raw FIFO chunks), C
  (physical qualification against a known source), and D (GDO0 hardware
  revision, blocked on a pin-allocation decision); the
  `subghz_arbiter.c`/`radio_arbiter.c` near-duplicate and the three
  now-near-identical SD loggers (reuse debt, not bugs); `check_rx_only.py`'s
  missing structural opcode-audit for CC1101 (LoRa has one); expanding
  `ocp_repl.py --gate-stop` to exercise the LoRa/CC1101 exclusion on real
  hardware.

  **2026-09-22, SDR cross-reference session.** Root-caused the whole
  night's erratic carrier-sense behavior: `REG_AGCCTRL1` was `0x1B`,
  recalled from training data without sourcing — the real TI datasheet
  (fetched and verified directly) puts AGCCTRL1 at `0x1C`; `0x1B` is
  AGCCTRL2. Every earlier "carrier-sense tuning" attempt had been
  scrambling front-end gain fields, not carrier-sense at all. Fixed the
  address, redesigned the data-rate target around a real, external
  ground-truth device (a LaCrosse-TX141THBv2 weather sensor, confirmed
  independently via an RTL-SDR + `rtl_433` on the same bench) — 26000 baud
  (achieves 25985) oversamples its 232 µs/460 µs pulses by exactly 6/12
  bits, replacing the untested 2.4 kBaud target. Swept both built-in
  relative carrier-sense thresholds (14 dB, then 6 dB, the least strict
  option) against SDR-confirmed transmission windows: zero chunks
  captured at either setting, with two and four confirmed real
  transmissions respectively inside the capture windows. Swapped the bench
  antenna for a proper 433 MHz whip (was a micro stub): no change, ruling
  out a simple antenna mismatch. Added a temporary unconditional RSSI
  diagnostic (`rssi_diag`, reads `REG_RSSI` every poll tick independent of
  carrier-sense/FIFO gating, since RSSI is normally only visible alongside
  an already-captured chunk) and time-correlated it against three further
  SDR-confirmed transmissions: **none produced any measurable RSSI bump**
  — the CC1101's own signal strength reading shows no distinguishable
  signature at any of the five confirmed real transmission timestamps
  checked tonight. Separately, the RSSI baseline itself drifted ~10-15 dB
  over a 3-minute idle window with nothing eventful happening, which would
  defeat a delta-based carrier-sense trigger on its own even if the real
  signal were visible. Neither anomaly is explained yet — leading
  candidates are receiver-sensitivity margin at the current gain/profile
  and/or AGC settling instability at 26 kBaud/203 kHz bandwidth, but both
  are unconfirmed. The `rssi_diag` instrumentation is being kept in the
  driver (proved genuinely useful, not a throwaway) rather than reverted.

  **Not yet hardware-confirmed:** whether the CC1101 can detect this or
  any real 433 MHz device at all under any carrier-sense configuration —
  tonight ruled out the register-address bug, the data-rate/bandwidth
  mismatch, and antenna mismatch as the explanation, but the underlying
  no-signal/drifting-baseline finding is unresolved. Will has a
  purpose-built CC1101 module (a Flipper Zero 400 MHz sub-GHz add-on, same
  chip, professionally matched PCB antenna circuit) to use as a baseline
  comparison next session — isolates whether tonight's breadboard
  construction (a plausible cause: breadboard parasitics are a known
  problem for RF above ~50 MHz, and could equally explain the drifting
  baseline as marginal/unstable contact) is the root cause, independent of
  the driver/register configuration. The new deck screen is still
  unverified end-to-end on the physical display. These are the first
  things to check next bench session.

**Exit gate:** every DESIGN §7.2 view backed by real frames; a wardrive session produces a valid WiGLE-importable CSV and a KML track.

---

## P8 — Polish

**Work:** config persistence (NVS + deck), higher-baud negotiation if needed, D-UCB channel picker ([D-6](docs/DECISIONS.md)), battery/UX pass, Log-view session browsing, error-surface review.

- [x] *(started 2026-09-17)* Deck UI foundation: brand semantic theme, shared deck chrome, grouped route metadata, and the SYSTEM/LINK → OBSERVE/WIFI SCAN → AP DETAIL first slice. See [`docs/brand/README.md`](docs/brand/README.md) and [`WORKLOG.md`](WORKLOG.md).
- [x] Migrate the remaining view renderers onto the shared shell with grouped titles and common controls.
- [ ] Validate the rendered result and keyboard behavior on the Cardputer hardware.

**Exit gate:** a field session start-to-finish on battery without a bench.

---

## Parallelism & critical path

- **Critical path:** P0 → P1 → P2 → P6 → P7.
- **P3 (LoRa)** and **P4 (GNSS)** branch off after P2 and can proceed in parallel with each other.
- **P5 (TFT)** is gated by hardware arrival, not by other phases (needs only P2).
- **P6** is the join point — it needs P2–P5 present to measure the real combined load.
