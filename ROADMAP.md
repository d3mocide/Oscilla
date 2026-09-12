# Oscilla — Roadmap

> Master phase tracker. Supersedes the DESIGN.md v0.1 milestone table.
> **How to read this:** each phase has an **entry gate** (what must be true to start), the **work**, and an **exit gate** (the demonstrable thing that says it's done). A phase is not "done" until its exit gate is met on hardware. Update **Status** and the date as phases move; log the detail in [`WORKLOG.md`](WORKLOG.md).

| Field | Value |
|---|---|
| **Current phase** | P1 complete → **P2 — Probe sees Wi-Fi** |
| **Last updated** | 2026-09-12 |
| **Hardware authority** | [`Research/c5-backpack-design.md`](Research/c5-backpack-design.md) Rev D |
| **Design authority** | [`DESIGN.md`](DESIGN.md) v0.2 |

---

## Status board

| Phase | Name | Status | Exit gate met? |
|---|---|---|---|
| **P0** | Reconcile & scaffold | 🟢 Exit gate met | ✅ 2026-09-12 |
| **P1** | Prove the link | 🟢 Exit gate met | ✅ 2026-09-12 |
| **P2** | Probe sees Wi-Fi | ⚪ Not started | — |
| **P3** | LoRa (RX) | ⚪ Not started | — |
| **P4** | GNSS on the deck | ⚪ Not started | — |
| **P5** | External TFT | ⚪ Not started | — |
| **P6** | Combined soak & power | ⚪ Not started | — |
| **P7** | Passive suite completion | ⚪ Not started | — |
| **P8** | Polish | ⚪ Not started | — |

Legend: ⚪ not started · 🟡 in progress · 🟢 exit gate met · 🔴 blocked

---

## P0 — Reconcile & scaffold

**Why first:** the two source documents described different machines; the protocol header is the one artifact both firmwares compile against, so it must exist and be agreed before either firmware starts.

**Entry gate:** repo exists (met).

**Work:**
- [x] Reconcile DESIGN.md against backpack Rev D → v0.2.
- [x] Create ROADMAP.md, WORKLOG.md, docs/DECISIONS.md.
- [x] Monorepo skeleton per DESIGN.md §10 (`protocol/`, `firmware-c5/`, `firmware-cardputer/`, `tools/`, `docs/hardware/`).
- [x] `protocol/ocp.h` v1 — 29 verbs, marker registry, proto version, caps, error codes, limits. Carries an `OCP_VERB_TABLE` X-macro so the probe's dispatch table and the tooling derive from one list, and a `#error` tripwire that refuses to build if a transmit flag is defined (D-8).
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
- [ ] `radio_arbiter.c` — PHY-lane single-owner mutex + teardown hooks + the (conservative) power interlock stub.
- [ ] `wifi_recon.c` — managed `scan_networks`, `show_scan_results`, passive `inspect_network <i>` (MFP + uptime).
- [ ] Frame emission: `[SCAN]` CSV rows, `[INSPECT]`.
- [ ] Deck: **Sweep** + **Trace** views over real frames.

**Exit gate:** `scan_networks` and `inspect_network` verified via `ocp_repl.py` against a live AP, then the same data rendered in Sweep/Trace on the deck. `stop` always returns to idle.

---

## P3 — LoRa (RX)

**Why here:** it's the second-riskiest bring-up (Rev D §4.3 — BUSY, RF_SW/DIO2 coherence, TCXO). Receive-only per [D-8](docs/DECISIONS.md) — no transmit. Blocked on [D-10](docs/DECISIONS.md) (TCXO startup delay, needed even to init the radio); RX params otherwise self-chosen.

**Entry gate:** P2 exit met. Wio harness assembled and continuity-checked per Rev D §4.1. A matching 862–930 MHz antenna attached before powering the radio (protects the front end even in RX).

**Work:**
- [ ] `lora_radio.c` (RX path) — reset sequence, bounded BUSY waits (fault, never hang), DIO1 ISR → radio task, RF_SW/DIO2 set coherently for **receive**, TCXO via DIO3 with documented delay, DC-DC mode, SPI ~1 MHz then raise.
- [ ] `lora_recon.c` RX — `lora_config`, `lora_listen`, `lora_status`; stream `[EVT] kind=lora`; framing classification (meshtastic/lorawan/unknown).
- [ ] Deck: **Sub-GHz** view (RX).
- [ ] Verify the build advertises `lora_rx` and that no transmit verb exists anywhere in the command table (the §8 guarantee).

**Exit gate:** observed real sub-GHz packets with RSSI/SNR in the Sub-GHz view; `stop` cleanly releases the LoRa lane; a grep of the built command table confirms zero TX verbs.

---

## P4 — GNSS on the deck

**Entry gate:** P2 exit met (independent of P3; can run in parallel).

**Work:**
- [ ] Deck GNSS UART (GPIO13 TX / GPIO15 RX, 9600 8N1 NMEA), distinct hardware UART from Grove.
- [ ] NMEA parse; fix validity + age; **no-fix ≠ no-UART-data** as distinct states.
- [ ] Configurable baud (a preconfigured unit may differ, Rev D §6). No UBX assumptions.
- [ ] Model: current-fix service feeding the logger.

**Exit gate:** live fix acquired outdoors and shown with age; unplugging the antenna shows "no fix" while UART stays alive; both states logged distinctly.

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

**Work:**
- [ ] `ble_recon.c` — NimBLE passive scan, device table, tracker classification; **Beacons** view.
- [ ] `zig_recon/` lifted verbatim (MIT) + `[ZIG]` frames; **Mesh** view.
- [ ] Sniffer/spectrum: `start_sniffer`, `show_clients/probes`, `channel_view`, `packet_monitor`, `deauth_detector`; **Contacts** + **Spectrum** views with `[EVT]` streaming.
- [ ] Wardrive: stream observations, deck-side geotag against local fix + age, write WigleWifi CSV + KML; **Drive** view.
- [ ] `start_antisurveillance` (deck correlates BLE sightings with its own movement).

**Exit gate:** every DESIGN §7.2 view backed by real frames; a wardrive session produces a valid WiGLE-importable CSV and a KML track.

---

## P8 — Polish

**Work:** config persistence (NVS + deck), higher-baud negotiation if needed, D-UCB channel picker ([D-6](docs/DECISIONS.md)), battery/UX pass, Log-view session browsing, error-surface review.

**Exit gate:** a field session start-to-finish on battery without a bench.

---

## Parallelism & critical path

- **Critical path:** P0 → P1 → P2 → P6 → P7.
- **P3 (LoRa)** and **P4 (GNSS)** branch off after P2 and can proceed in parallel with each other.
- **P5 (TFT)** is gated by hardware arrival, not by other phases (needs only P2).
- **P6** is the join point — it needs P2–P5 present to measure the real combined load.
