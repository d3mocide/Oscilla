# Oscilla — Roadmap

> Master phase tracker. Supersedes the DESIGN.md v0.1 milestone table.
> **How to read this:** each phase has an **entry gate** (what must be true to start), the **work**, and an **exit gate** (the demonstrable thing that says it's done). A phase is not "done" until its exit gate is met on hardware. Update **Status** and the date as phases move; log the detail in [`WORKLOG.md`](WORKLOG.md).

| Field | Value |
|---|---|
| **Current phase** | P5 complete → **P6 — Combined soak & power** (🟡 periodic use pending); P7/P8 continue in parallel |
| **Last updated** | 2026-11-21 (v0.3.0+) |
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
| **P5** | External TFT | 🟢 Exit gate met (operator-reported, see P5) | ✅ 2026-11-20 |
| **P6** | Combined soak & power | 🟡 In progress — mode switching passed; periodic use, battery soak, power budget open | — |
| **P7** | Passive suite completion | 🟡 In progress (out of sequence) | — |
| **P8** | Polish | 🟡 In progress — deck UI foundation | — |
| **P9** | Field intelligence & RF enhancements | ⚪ Not started (additive, receive-only) | — |

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
- [x] Deck: **Wi-Fi Scan** + **AP Detail** views over real frames — `app/deck_app`, `model/scan_model`, `ui/sweep_view`, `ui/wifi_detail_view`; auto-paging; 16 KB drained RX buffer.

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

**Follow-up — LoRa Session Integrity (2026-09-22):** P3 reception remains
complete; its session-provenance, health-counter, SD-record, and soak follow-up
is tracked separately in [`docs/lora-session-integrity.md`](docs/lora-session-integrity.md).
It does not expand P3's receive-only capability surface or re-open the P3 exit
gate.

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
- [ ] Stationary/moving wardrive follow-up (2026-11-17): cooperative row draining/fresh-fix guards address whole-page SD stalls. Stationary comparison passed with a larger indoor test antenna: 64 s idle + 121 s logging, 778 AP observations, zero UART/NMEA/write errors; RMS wander 3.8 m idle / 3.0 m logging. Final-case antenna, outdoor/moving route and longer soak checks remain open.
- [ ] Runtime baud setting. Still a compile-time constant (`kGnssBaudDefault` in `main.cpp`). The bench confirmed the delivered unit is 9600 as shipped, so a settings UI for a value that's never needed changing stays speculative work — revisit only if a different unit ever requires it.

**Exit gate: MET — 2026-09-16.** All three requirements demonstrated on real hardware in one session: a live fix acquired outdoors and held continuously for 21+ minutes with age shown (`age_ms` fresh every ~5s, zero drops); unplugging the antenna produced `fix lost` — not `no data` — while NMEA sentences kept arriving over UART (`chkfail`/`overlong` stayed at 0 through the drop, confirming the link itself never broke); reconnecting recovered cleanly back to `fix`. The wardrive session logged through the same window confirms the CSV/KML side of the gate too: every fixed observation was written, an earlier all-no-fix session (`drive_0003`) confirmed the `nofix` counter and empty CSV path both work when there's never a fix at all. Full detail in WORKLOG 2026-09-16.

**Follow-up 2026-09-17:** an indoor, poorly placed receiver remained in `fix lost`; the cumulative `chkfail=63` and `overlong=0` counters stayed flat for a 90-second read-only capture, with no new UART error. This is not a reproduced parser fault. A fresh outdoor, undisturbed receiver run remains the appropriate follow-up to the earlier connector disturbance.

---

## P5 — External TFT

**Why late:** waiting on the panel to arrive; and the shared SD/TFT SPI bus (Rev D §5.2) is the deck's hardest correctness problem.

**Entry gate:** panel in hand. P2 exit met.

**Work:**
- [x] ILI9341 write-only landscape init, MISO unconnected, 4 MHz; delivered no-CS variant uses Rev D §5.3 pins and corner-pixel parking. Colour/GPS screen observed on hardware 2026-10-09.
- [x] Replacement GMT028-05/ST7789 colour fixture: user confirmed colours on 2026-11-16; normal-app profile uses TFT 8 MHz / SD 8 MHz (Rev D §5.4).
- [x] Native ST7789-only normal-app output and corrected colour polarity confirmed by user, 2026-11-16.
- [x] Replacement ST7789 native output/colours confirmed; 8 MHz TFT/SD passed 32 write/read/cleanup cycles (128 KiB), with clean redraw confirmed by user, 2026-11-16.
- [x] Qualify physical cold boot with SD and sustained soak on the replacement ST7789. Operator-reported 2026-11-20: repeated cold boots with SD inserted, and a ~10 h soak on USB power with no display or bus corruption. No captured log; battery operation is not part of this gate (P6/P8).
- [x] Review the graphical companion overhaul on-device: Packet Monitor scope first, then populated/empty/cached states across the other cards. Operator accepted Sniffer visuals, the Packet Monitor redesign and System resource monitor (2026-11-17), and accepted the expanded external views as complete with the Observatory v2 revision (2026-11-20).
- [x] **Single shared-bus lock** covering SD + TFT; SD brought up first. Interleaved fixture passed 32 cycles / 128 KiB readback; user confirmed clean display after parking. Delivered variant caps SD at 8 MHz.
- [x] Expanded views on the 240×320: delivered with Observatory v2.
- [~] ~~Second-panel detection at boot~~ — dropped 2026-11-20. The panel is write-only (no MISO readback), so it cannot be detected, and it is now a standard fitment rather than optional (DESIGN §7.4).
- [x] Touch stays disabled (Rev D baseline): the GMT028-05 has no touch controller and the deck has no touch code.
- [ ] **External display power gate** (deferred, 2026-11-22): idle sleep leaves the GMT028-05 backlight lit and white (no backlight pin). Proposed harness: GPIO4 `DISP_EN` switching the panel VCC branch plus a 74LVC126 isolating its signal lines from the shared SD bus — [`docs/hardware/display-power-gate.md`](docs/hardware/display-power-gate.md). Measure panel current first; firmware power-off/re-init follows the harness.

**Exit gate:** sustained TFT redraw concurrent with SD read/write of a disposable file, cold-boot with SD inserted, no bus corruption over a soak.

2026-10-09: disposable-file/display gate demonstrated in the bounded fixture.
Normal-app probe packet/health logging and SD readback also demonstrated on
2026-10-09. First mockup-based Packet Monitor companion implemented; physical
layout review, lower-clock long soak, and controlled cold-boot/JTAG qualification
remain open; P5 is not declared complete.

2026-11-20: **exit gate met.** The bounded fixture covers concurrent redraw
plus disposable-file SD I/O; the operator reports repeated cold boots with SD
inserted and a ~10 h USB-powered soak with no bus corruption. The soak and
cold-boot evidence is operator-reported, not a captured log. Closed in v0.3.0.

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

2026-11-17: targeted heap regression sampled 30 min 17 s across two recording
segments (USB sampler interruption explicitly recorded). Passive Wi-Fi + LoRa
SD logging + GNSS + paired session/System UI completed 24 review cycles.
Probe free heap and both largest blocks stayed constant; deck recovery heap
remained bounded. Zero observed reset/UART/write/log-drop faults. This does
not close the uninterrupted multi-hour soak or measured-power gates; see WORKLOG.

2026-11-19: recovered the earlier endurance attempt's USB-disconnect outcome
(last sample 990.38 s after loaded baseline; no phase completed). Restarted
the two-hour steady, 30-minute passive mode-switching and one-hour powered-on
use/idle sequence with count-only automatic reports and sparse follow-up.
The steady phase completed 7,200.50 s without observed reset/error/drop faults;
probe current heap stayed constant and deck post-warm-up heap remained bounded.
Mode switching then failed after 7 min 17.52 s: deck allocation abort in contact
snapshot vector growth, with only 7,796 B in the largest free block at the last
sample. Periodic use never started; recording closure was unconfirmed. GNSS
acquired a fix after baseline. Fix/retest mode-switch memory pressure before
claiming combined endurance; measured power and P5/P6 exit gates remain open.

2026-11-20: removed overlapping contact/BLE snapshot table allocations and
released inactive external-display model copies while retaining app caches.
Host/mutation checks, both normal firmware builds and stack guards passed;
updated deck flash verified. Retest completed eleven mode slots, including both
sniffer slots, then aborted on AP-vector growth in the final Wi-Fi live slot
(1,712.19 s overall). Switching still fails; periodic use and the new steady
soak never started. Bound remaining AP/mixed-mode allocation peaks and retest.
No new-firmware endurance gate is claimed; recording closure was unconfirmed.

Follow-up: AP inventory now grows in small blocks instead of reallocating a
contiguous array. Host full-capacity/live/paged/frozen-copy tests reject blocks
over 4 KiB; mutation restores the failure. Normal firmware builds and stack
guards pass, deck flash verified. First-test-only run PASSED all twelve mode
slots in 1,800.43 s, reported clean recording closure and 120.13 s idle recovery.
Zero timed errors/resets/panics/drops/retries; minimum free loop stack 2,816 B.
Idle deck heap stayed within 99,376–99,464 B; probe recovered to 99,172 B.
Periodic use and the new-image long soak remain unrun and did not automatically
start. This first-test pass does not close measured-power or P5/P6 gates.

Subsequently authorized periodic/long attempt was interrupted by USB loss:
one full cycle completed, then disconnect during cycle 2 idle (last sample
1,029.93 s overall). Both use recordings had closed before loss; no sampled
firmware/protocol/storage fault preceded it. Host kernel logged repeated USB
re-enumeration and `-71` errors. Periodic qualification remains incomplete and
the new-image two-hour stage never started. Check USB stability and rerun from
a fresh baseline; the separate first mode-switching pass remains valid.

2026-11-20 (v0.3.0): **periodic use and two-hour soak PASSED** — 10,879 s,
6/6 use/idle cycles then 7,200 s steady passive Wi-Fi + LoRa RX/SD logging,
zero errors/resets/drops, no reset on either board, bounded heap on both. Probe
was Grove-powered from the deck (operator choice, D-4 still open); deck on a
direct USB port. Battery soak, measured current/supply dips and the interlock
remain open. See WORKLOG.

2026-11-20 (v0.3.0): entry gate now met with P5 closed. Next: check the USB
cable/hub path, rerun periodic use from a fresh baseline, run the new-image
two-hour soak, then a battery-powered soak and the measured current budget.

**Exit gate:** a documented current budget in `docs/hardware/`; the arbiter's interlock reflects it; a clean multi-hour soak. Only after this does Grove-powered operation ([D-4](docs/DECISIONS.md)) get evaluated.

---

## P7 — Passive suite completion

**Entry gate:** P2 + P6 exits met.

P7 work is being exercised out of sequence while the external TFT and measured
power gates are pending. The items below distinguish implementation and
receive-only RF evidence from the final phase exit gate; P7 is not declared
complete until its remaining real-frame and view checks are demonstrated.

**Work:**
- [x] **BLE (2026-09-16).** `ble_recon.c` — NimBLE passive scan (`scan_bt`/`scan_airtag`, OCP-SPEC §11), device table, Find My/AirTag tracker classification (Apple company ID `004c` + type byte `0x12`, confirmed against public Find My protocol write-ups). Split probe-side into `ble_adv_parse.c` (AD-structure parsing) and `ble_device_table.c` (upsert logic), both pure C and host-tested (13 + 14 checks, plus 200k fuzz iterations under ASan/UBSan) — same shape as `beacon_parse.c`/`sniff_track.c`. Deck-side `src/model/bt_model` + **BLE Scan** view (`src/ui/bt_view`), 16 host tests. Enabling NimBLE overflowed the default 1 MB app partition by ~126 KB; fixed with Espressif's own "large single-app" table (1500 KB) rather than trimming anything — 8 MB of physical flash made that the easy call. Both firmwares build clean on the real toolchain (probe now at 76% of its larger partition). **Hardware-confirmed same day.** `scan_bt`/`scan_airtag` both verified against real BLE traffic (70–75 real devices per scan, 0 malformed rows) after fixing two real bugs a client-timeout mismatch (`scan_bt`'s reply arrived after the deck's generic 2s timeout had already given up — added `kBleScanTimeoutMs`) and a NimBLE semantics bug (`ble_gap_disc_cancel()` doesn't emit a completion event the way `esp_wifi_scan_stop()` does, so cancelling `scan_airtag`'s unbounded scan left the radio arbiter stuck forever — fixed with an idempotent `finish_scan()` called from both the natural-completion and forced-cancel paths). Full detail in WORKLOG. **Display confirmed 2026-09-22:** the BLE Scan card viewed directly on the Cardputer's own screen during a live scan — operator-confirmed clean rendering (Will, visual check, no automated capture). **Still open:** no real AirTag on hand to confirm the tracker classification against genuine hardware. The in-house 802.15.4 recon slice is now planned under D-17.
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
- [x] `channel_view`, `packet_monitor` + **Packet Monitor** view — same early/out-of-sequence batch. **Hardware-confirmed 2026-09-16**: `spectrum` and `channel <n>` both ack'd by the real probe over the debug console (9 real readings, `cfg ack ch=6`). **Display confirmed 2026-09-22:** started a channel scan and locked into packet-monitor mode on a selected channel directly on the Cardputer's own screen — operator-confirmed clean rendering (Will, visual check, no automated capture).
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
- [ ] **Band & channel filtering (`set_band` / `set_channels`).** Implemented on probe (`wifi_channels.{h,c}`, `wifi_recon.{h,c}`, `ocp_server.c`) and deck (`wifi_band_sync.h`, SD `settings`, Settings `b` key, `band`/`channels` debug commands, OCP-SPEC §10.9); host/ASan tests, mutation proof, and both firmware builds pass. **Hardware-confirmed 2026-11-21:** `band 24` (67 APs, 2.76 s), `band 5` (66 APs, 7.03 s), `channels 1,6,11` under `band=auto` (61 APs, **755 ms** after narrowing ESP-IDF's band mode for single-band bitmaps), `channels 52` cleanly refused (`code=badarg`), and mid-tool `band` change deferred cleanly until `stop phy` with 0 hop/link errors. **Still open:** the 5-minute bench check for 5 GHz passive DFS channels (`52–144`) to settle [D-14](docs/DECISIONS.md).
- [ ] **Probe-driven multi-band wardrive (`start_wardrive`).** Implemented on probe (`wardrive_cache.{h,c}`, `wifi_wardrive.{h,c}`, `ble_recon_wardrive_window()` via single audited `start_disc()`, `PHY_OWNER_WARDRIVE`, OCP-SPEC §10.10) and deck (`wardriveCsvBtRow()` 14-column WigleWifi-1.6 `Type=BLE` + `MfgrId`, `session_review.cpp` `Type=BLE` support, `wardriveLogBt()`, bounded receipt-time geotagged Wi-Fi/BT queues in `deck_wardrive.cpp` with automatic fallback to `scan_networks` on older probes, and Drive view AP+BT counts). ASan/UBSan host tests, mutation proofs, `check_rx_only.py`, and both firmware builds pass. **Still open:** live moving multi-band bench/field verification once hardware is reflashed.

- [ ] **CC1101 433 MHz receiver (D-15, reopened 2026-11-23; branch `cc1101-idf`).** Clean-room receive-only `cc1101_{bus,profile,config,rx}.c` with 348 host checks under ASan/UBSan and mutation proofs. It is qualified on an S3 bench through `bench-cc1101-driver`: SDR-confirmed LaCrosse 6/6, Flipper 5/5, the same result as a SmartRC reference; 25,700 bring-up/RX cycles; a 30-minute soak; 2,294 cold starts; no failure. The C5 port shares the bus with the SX1262 under `subghz_spi.c`, and the `--cc1101-bench` build serves the same console on USB. Pins are in Rev D §4.4. **C5 harness validated 2026-11-23:** floor −89 dBm with the SX1262 idle and sweeping; Flipper 15/15 and LaCrosse 6/6 received under load; 0 SX1262 sweep errors in 30k; boot and USB flash OK with the pull-up. **Bitstream over SPI, no GDO0 wire (2026-11-23):** `cc1101_capture` drains the FIFO losslessly at 25 kbaud under SX1262 load, and `ook_decode` decodes Princeton and LaCrosse TX141THBv2. Live, it matched rtl_433 14/14 and Flipper 40/40, with 0 false packets. The GDO0 wire is not needed. **OOK lane over OCP (D-20, 2026-11-24):** `ook_listen`/`ook_status`/`stop ook`, `kind=ook` events with repeat-merging and rate-limited `ook_unknown` summaries, concurrent with the PHY and LoRa lanes; `--gate-ook` 22/22 and LaCrosse values identical to rtl_433 with all three lanes live. **Still open:** JTAG attach; the deck view and the deck's OCP client for `ook=`/`kind=ook`. The September archive stays on branch `cc1101`.

**Exit gate:** every DESIGN §7.2 view backed by real frames; a wardrive session produces a valid WiGLE-importable CSV and a KML track.

---

## P8 — Polish

**Work:** config persistence (NVS + deck), higher-baud negotiation if needed, D-UCB channel picker ([D-6](docs/DECISIONS.md)), battery/UX pass, Log-view session browsing, error-surface review.

- [x] *(started 2026-09-17)* Deck UI foundation: brand semantic theme, shared deck chrome, grouped route metadata, and the SYSTEM/LINK → OBSERVE/WIFI SCAN → AP DETAIL first slice. See [`docs/brand/README.md`](docs/brand/README.md) and [`WORKLOG.md`](WORKLOG.md).
- [x] Migrate the remaining view renderers onto the shared shell with grouped titles and common controls.
- [ ] Paired AP/BLE dossiers: identity-pinned inspection, cached client relationships and real-observation RSSI history implemented; rendered previews and host checks available, operator accepted normal paired dossier appearance; remaining error-state review pending.
- [ ] Saved-session browser implemented: bounded read-only SD snapshots, summaries, paged wardrive/LoRa records and saved packet hex. Host/source-render and real-SD bench checks passed (778 wardrive rows, 46 LoRa packets, paging/hex/Back, reception preserved); operator layout review pending.
- [ ] Validate the rendered result and keyboard behavior on the Cardputer hardware.
- [x] **Non-blocking `lora_listen` startup:** investigated and tested live on connected hardware (2026-11-21) across 6/6 peak-burst `wifiscan` + `lora_listen` cycles: ~140 ms round-trip, `link_timeouts=0`, zero drops. The original 2026-11-20 timeout was root-caused to the deck's O(N²) `SD.exists()` session-numbering stall inside `loraLogBegin()`, already fixed in `858e6e7`; no probe dispatch refactor needed.
- [ ] **Idle power saving:** implemented deck-side idle display sleep (`app/idle_sleep.h`, `OFF`/`30s`/`60s`/`2m`/`5m` persisted to `/oscilla/idle_timeout`, `t` key on Settings + `idle` debug command): blanks internal LCD (`setBrightness(0)` + `sleep()`) and puts the external ST7789 into `SLPIN` under `storage::lock()` (stopping SPI2 redraws; `BLK` stays on 3.3 V per Rev D §5.1) while keeping GNSS, Grove OCP, and SD logging alive; any key wakes and is swallowed. **Hardware-confirmed 2026-11-21:** `idle 30` entered `display_sleeping=1` / `tft_status sleeping=1` during live `wifiscan` (161 APs, GNSS fix `age_ms=59`, `link_timeouts=0`), and pressing `` ` `` woke both displays while swallowing the key (`wifi_live=1` preserved). **Still open:** P6 current-delta measurement and deeper deck sleep evaluation.
- [ ] **Persistent deck & probe settings:** deck SD `storage::settings` persists debug mode, brightness, Wi-Fi region, Wi-Fi band scope, idle display timeout, default LoRa RX profile (`/oscilla/lora_profile`), and audio cues (`/oscilla/audio_enabled`, off/muted by default), all editable from the overhauled 7-row paired Settings menu (`settings_view.cpp` + Observatory v2 320×240 inspector). Probe NVS persistence (`config_codec.{h,c}` + `config.{h,c}`, CRC32-protected 128 B blob in `oscilla`/`cfg_v1`) now persists Wi-Fi region, band scope, custom channels, and last applied LoRa RX parameters across probe reboots (ASan/UBSan host tests, mutation proofs, and both firmware builds pass; reboot persistence bench verification pending next probe flash).
- [x] **D-UCB adaptive channel picker ([D-6](docs/DECISIONS.md)):** Implemented as pure-C `wifi_channel_picker.{h,c}` (discounted UCB1 with initial full sweep, starvation-freedom, and non-stationary shift adaptation) and wired into `wifi_sniff.c`, `wifi_networks.c`, and `wifi_deauth.c` (`wifi_spectrum.c` stays uniform round-robin). Host/ASan tests, mutation proof, and both firmware builds pass. **Hardware-confirmed 2026-11-21:** live 7 s sniffer (790 pkts, 10 clients, 3 probes), 7 s continuous Wi-Fi scan (141 APs), and 3 s deauth detector all ran with zero hop errors, timeouts, or resets on the probe console.
- [ ] **LOGS sidecar inspection & session waypoints:** implemented cooperative read-only `.meta` + `.health.csv` sidecar parsing (`LoraMetaParser` / `LoraHealthParser` in `session_review.{h,cpp}` + bounded <= 512 B/turn reading under `lock(0)` in `session_reader.cpp`), surfaced tuning/health/geotag counts in `session_view.cpp`, and added manual session waypoints (`m` key on Drive & Sub-GHz + `mark` debug command, `sWaypoint` KML placemarks). **Hardware-confirmed 2026-11-21:** live `mark` wrote `waypoint drive=0 lora=1`, and opening the recorded LoRa session in LOGS loaded `session_parts=15` (`CSV|KML|META|HEALTH`), `session_meta=1`, `session_health=1`, `session_geotagged=1`, `session_valid=1`, `session_bad=0`, `session_error=0` with `0` timeouts/errors. **Still open:** on-device visual layout review.
- [ ] **Measured battery burn-rate / runtime estimate:** implemented pure-C++ `model::BatteryTrend` in `system_resources.h` (requires >= 60 s elapsed and >= 2% drop while discharging; resets on charging/bounce; clock-wrap safe), wired to periodic `M5.Power` sampling in `DeckApp::tick()`, onboard `info_view.cpp` (`BAT` / `EST`), and `dump`. Host tests, mutation proof, both firmware builds, and live `M5.Power` telemetry (`battery_pct=100`, USB bench) pass. **Still open:** unplugged battery-discharge slope verification alongside the P6 power interlock.

**Exit gate:** a field session start-to-finish on battery without a bench.

---

## P9 — Field intelligence & RF enhancements (post-v1)

**Why here:** additive, strictly receive-only capabilities ([D-8](docs/DECISIONS.md)) that deepen RF inspection, tracking, and offline analysis without delaying v1's battery/soak gates. Pure-C parser slices and host-tested models may be prototyped ahead of gate closure as long as they do not disrupt P6–P8 stability.

**Entry gate:** P6–P8 exits met (host-tested parser slices may start earlier).

**Work:**
- [ ] **Richer 802.11 beacon & RSN inspection (`beacon_parse.c`):** Implemented on probe (`beacon_parse.{h,c}`, `wifi_inspect.c`) and deck (`scan_model.{h,cpp}`, `wifi_detail_view.cpp`, OCP-SPEC §10.3): extracts RSN AKMs (`PSK`/`SAE`/`EAP`/`FT-*`/`OWE`), channel width (`20/40/80/160 MHz`), Wi-Fi generation (`4/5/6`), Country IE (`cc`), and WPS vendor IE (`00:50:F2:04`), with ASan/UBSan tests, mutation proof, and both firmware builds passing. **Bench smoke-tested 2026-11-21:** live `inspect 1` captured 4 beacons cleanly with zero link/parse errors. **Still open:** visual confirmation of the enriched dossier line on the deck screen across WPA3-SAE / Wi-Fi 6 / WPS APs.
- [ ] **Multi-vendor BLE tracker classification (`ble_adv_parse.c`):** Implemented on probe (`ble_adv_parse.{h,c}`, `ble_device_table.{h,c}`, `ocp_frame.{h,c}`, `ble_recon.c`) and deck (`bt_model`, `anti_surveillance_model`, `bt_view`, OCP-SPEC §11.4): classifies Apple Find My (`airtag`), Google Find My Device Network (`fmdn`), Samsung SmartTag (`smarttag`), Tile (`tile`), and Chipolo (`chipolo`), with ASan/UBSan tests, mutation proof, and both firmware builds passing. **Bench smoke-tested 2026-11-21:** live `scan_bt` saw 71 devices / 13 trackers and `scan_airtag` logged 8 sightings in 3 s with zero errors. **Still open:** controlled verification against physical non-Apple hardware trackers (FMDN, SmartTag, Tile, Chipolo).
- [ ] **SX1262 sub-GHz passive energy / noise-floor sweep:** implemented on probe (`OP_GET_RSSI_INST` `0x15` allowlisted in `lora_radio.c` & `check_rx_only.py`, pure-C `lora_sweep_plan.{h,c}`, `lora_cmd_sweep()` in `lora_recon.c`, `OCP_V_LORA_SWEEP` / `[EVT] kind=lora_sweep`, OCP-SPEC §13) and deck (`model/lora_sweep_model.{h,cpp}` with 64-bin peak-hold + 24-row waterfall ring, `w` toggle on Sub-GHz screen with US 902 / EU 868 / ISM 433 presets, and Observatory v2 320×240 Sub-GHz Spectrum & Waterfall view in `external_lora_view.cpp`). ASan/UBSan host tests, mutation proofs, `check_rx_only.py`, view render, and both firmware builds pass. **Still open:** live SX1262 hardware sweep timing & noise-floor calibration check.
- [ ] **SD-loaded LoRa profiles & CAD-assisted profile hopping:** SD-loaded presets implemented (`model/lora_profile_catalog.{h,cpp}` parsing `/oscilla/lora_profiles.csv`, validated against SX1262 ranges, capped at 16 with built-in fallback, persisted default in `/oscilla/lora_profile` and exposed in the paired Settings inspector + Sub-GHz `x`/`c`; host tests, mutation proof, and both firmware builds pass). **Still open:** hardware verification with custom SD CSV and optional SX1262 `SetCad` watchlist hopping.
- [ ] **Geotagged LoRa coverage logging & KML export:** implemented 16-column geotagged LoRa CSV (`lat,lon,alt,hdop,fix_age_ms,utc`, blank when no fix; `session_review` accepts both legacy 10-col and 16-col CSVs) and lazy incremental SNR-colored `log_NNNN.kml` coverage logs (`sSnrGood` > 0 dB, `sSnrMid` -10..0 dB, `sSnrWeak` < -10 dB, `sWaypoint` markers) in `lora_log_format.{h,cpp}` and `lora_logger.{h,cpp}`. **Hardware-confirmed 2026-11-21:** live MeshCore packet logged with fresh GNSS fix (`lora_geotagged=1`, `lora_waypoints=1`, `verify_lora_log state=match`, `write_errors=0`, `link_timeouts=0`, and `session_parts=15` read back cleanly in LOGS). **Still open:** outdoor multi-point KML plot inspection.
- [ ] **Passive baseline & anomaly / evil-twin watch mode:** implemented pure-C++ `model/baseline_watch.{h,cpp}` + SD store `storage/baseline_store.{h,cpp}` (`/oscilla/baselines/default.csv`), wired to Wi-Fi Scan (`b` key to capture/clear, `baseline save|load|clear` debug command, onboard `BL OK`/`BL +N`/`BL !N` pill + tinted rows, and external Reading Dock `BL // EVIL TWIN?` / `AUTH DOWNGRADE` / `MFP DROPPED` / `NEW AP` banner) and live `[SCAN]`/`[EVT]`/`[INSPECT]`/`[ZIG]` paths. ASan/UBSan host tests, two mutation proofs, view render, and both firmware builds pass. **Still open:** on-device SD save/reload and live anomaly display verification.
- [ ] **Single-target "Signal Hunt" mode:** implemented on probe (`hunt_filter.{h,c}`, `wifi_hunt.{h,c}`, `ble_recon.c` `hunt_ble` via single audited `start_disc()`, `OCP_V_HUNT_WIFI` / `OCP_V_HUNT_BLE` / `[EVT] kind=hunt`, OCP-SPEC §14) and deck (`model/signal_hunt.{h,cpp}`, `ui/signal_hunt_view.{h,cpp}`, `ui/external_hunt_view.{h,cpp}`, persisted `/oscilla/audio_enabled` default-muted `AUDIO CUES` Settings row, `f` hotkey from Wi-Fi Scan / AP Dossier / BLE Scan / BT Dossier / Anti-Surveillance, `a` live audio toggle, `r` peak reset, and `M5Cardputer.Speaker.tone()` monotonic Geiger cadence). ASan/UBSan host tests, mutation proofs, `check_rx_only.py`, view renders, and both firmware builds pass. **Still open:** live RF hunt cadence, RSSI responsiveness, and speaker volume check on physical hardware.
- [ ] **Expanded muteable audio cues:** extend the persisted, default-muted `AUDIO CUES` setting beyond Signal Hunt's Geiger cadence with subtle, distinct cues for high-signal events — Deauth Detect burst chirp, Baseline/Evil-Twin high-severity chime, Anti-Surveillance `FOLLOW?` alert, LoRa RX packet blip, and GNSS fix-acquired / manual waypoint tick (with optional per-cue or volume control once speaker acoustics are tested on hardware).
- [ ] **Offline OUI & BLE Company-ID lookup tables on SD:** implemented sorted 24-byte binary format + O(log n) lookup & 32-slot LRU cache (`model/vendor_table.h`, `storage/vendor_lookup.{h,cpp}`), host builder (`tools/build_vendor_tables.py`), and dossier wiring (`wifi_detail_view.cpp`, `bt_detail_view.cpp`, clean fallback when `/oscilla/oui.bin` or `/oscilla/bt_mfr.bin` is absent). Host tests, Python↔C++ cross-check, mutation proof, and both firmware builds pass. **Still open:** on-device check with real IEEE/BT-SIG tables on SD.
- [ ] **Vendor database sync & SD log/wardrive offload (respecting [D-8](docs/DECISIONS.md)):** convenient update of `/oscilla/oui.bin` & `/oscilla/bt_mfr.bin` plus browsing/downloading SD session logs and uploading wardrive CSVs. Two clean options:
  1. *Zero-RF USB-C host sync utility* (v1-compatible, keeps [D-8](docs/DECISIONS.md) unbroken): a companion script on a laptop/phone over USB-C that refreshes vendor `.bin` tables, pulls `/oscilla/logs/`, and uploads WiGLE CSVs without adding any radio TX path to Oscilla.
  2. *Local Wi-Fi / hotspot Dock/Sync mode* (post-v1, requires explicitly revisiting [D-8](docs/DECISIONS.md) or isolating into a separate non-recon OTA partition / boot-time Dock utility so the field-recon firmware binary remains 100% receive-only with zero Wi-Fi/BLE TX symbols compiled in).
- [ ] **Post-handshake baud negotiation & bounded passive PCAP capture ([D-2](docs/DECISIONS.md)):** keep `115200 8N1` at boot for ROM-noise resilience, optionally negotiate `460800`/`921600` after `[HELLO]` (reverting on reset/timeout), and stream filtered passive management / 802.15.4 frames to deck microSD as standard `.pcap` files.

**Exit gate:** each shipped P9 slice passes host unit/fuzz tests, preserves `check_rx_only.py` (zero transmit paths), fits within the P6 heap/power envelope, and is demonstrated on real hardware.

---

## Parallelism & critical path

- **Critical path:** P0 → P1 → P2 → P6 → P7 → P8 → P9.
- **P3 (LoRa)** and **P4 (GNSS)** branch off after P2 and can proceed in parallel with each other.
- **P5 (TFT)** is gated by hardware arrival, not by other phases (needs only P2).
- **P6** is the join point — it needs P2–P5 present to measure the real combined load.
- **P9** items are independent vertical slices and can be prioritized in any order once v1 gates are healthy.
