## 2026-09-13 — Deck Sub-GHz view: lora_model + subghz_view, flashed and live

**Phase:** P3 · **By:** Will + Claude

- Added `model::LoraModel` (`firmware-cardputer/src/model/lora_model.{h,cpp}`) — same shape as `DeauthModel`: a capped, newest-first packet log absorbed from `[EVT] kind=lora`, since there's no snapshot-dump verb for LoRa either. Configured params (freq/sf/bw/cr) come from what the deck itself sent via `lora_config`, not parsed back out of the `[CFG]` reply — that marker is shared across several verbs and isn't decodable from the frame alone, same reasoning already documented for `OCP_MARK_CFG` in `deck_app.cpp`.
- Added `ui::drawSubGhzView` (`subghz_view.{h,cpp}`), a new **SubGhz** home card between Spectrum and Deauth in the nav cycle: `c` sends `lora_config` (MeshCore's USA/Canada preset, hardcoded for now — there's no numeric-entry UI yet, so this is a placeholder, not a protocol default; `ocp.h`'s `lora_config` still takes no default frequency, per D-9), `s` starts/stops `lora_listen`, `;`/`.` move the selection.
- Wired into `deck_app.h`/`.cpp` following the exact existing pattern (`Screen::SubGhz`, arbiter-less stop-on-leave like Spectrum/Deauth, `onEvent` absorption, `onReset` clears it like everything else).
- Host test `lora_model_test.cpp` (14 checks) added to `check_protocol.sh` alongside the other model tests — config-vs-parse separation, malformed-event rejection (hex length mismatch, missing SNR), the row cap, and stop/clear semantics.
- Built both firmwares clean (no warnings on any touched file), full `check_protocol.sh` suite green (80 source files now in the rx-only scan), flashed both boards: C5 back to the `uart` (Grove) build, Cardputer to the new deck build with the Sub-GHz card.
- **Confirmed live on the deck itself, same session:** cycled to the new SUBGHZ card on the Cardputer, pressed `c` then `s`, and watched real MeshCore packets scroll in through the actual UI — RSSI/SNR/length/hex, over Grove, not the bench transport. Full chain proven end to end: real chip -> `lora_radio.c` -> `lora_recon.c` -> Grove -> `ocp_client`/`LoraModel` -> `subghz_view`. This satisfies the P3 exit gate's "in the Sub-GHz view" clause that was the one open piece a moment ago.

---

## 2026-09-13 — First live SX1262 RX bring-up: no faults, on real hardware

**Phase:** P3 · **By:** Will + Claude

- Wired the Wio-SX1262 to the C5 point-to-point per Rev D §4 (confirmed pin mapping, not the withdrawn one — see the earlier entry below), **without the NSS/RST/RF_SW pull resistors** Rev D calls for: measured all three pads with a multimeter first (open/`OL`, not populated on the Wio board itself), then wired anyway as a deliberate bench-only call — RF_SW is a Wio *input*, not an actively-driving signal, so it doesn't fight the C5's GPIO25 strap sampling the way an external driver would have. Boot came up clean across the flash/reset cycles done today; no corruption observed.
- Wrote `lora_recon.c` — the OCP verb layer on top of `lora_radio.c`: `lora_config`/`lora_listen`/`lora_status`, an `[EVT] kind=lora` emitter, and a self-terminating drain task. Wired `lora_rx` into `ocp_server.c`'s capability advertisement and dispatch table, and into `main.c`'s boot sequence. `stop` releases the LoRa lane directly (not through `radio_arbiter`, which doesn't model a LoRa lane yet — DESIGN §6.2 defers that interlock to P6).
- Flashed the `--bench` build (native USB transport) and drove it live with `ocp_repl.py`:
  - `hello` → `caps=wifi24,wifi5,lora_rx` — `lora_radio_init()` succeeded on real hardware (SPI bus, GPIO, ISR, task all up).
  - `lora_config 915000000 7 125 1` → accepted, echoed correctly.
  - `lora_listen` → **the full bring-up sequence completed with zero faults**: NRESET pulse + bounded BUSY wait, `SetDIO3AsTCXOCtrl` (D-10's values), `Calibrate` + bounded BUSY wait (~3.5ms), `SetStandby(XOSC)`, DC-DC regulator mode, DIO2 RF-switch enable, packet type/frequency/modem/packet params, IRQ config, `SetRx` continuous — no `hwfault`, no `ESP_ERR_TIMEOUT` anywhere. `[STATUS]` showed `lora=rx`.
  - `stop` → `[STOP] running=0`, and `lora_status` confirmed `running=0` after — clean release, satisfying that part of the P3 exit gate on real hardware.
- **What this proves:** SPI (SCK/MISO/MOSI/NSS), NRESET, and BUSY are wired correctly and the chip is genuinely alive and responsive — this is a much stronger signal than a static continuity check would have given. Testing without the pull resistors turned out fine in practice, matching the reasoning that led to skipping them.
- **Update, same session:** retargeted to MeshCore's real USA/Canada preset — 910.525MHz, SF7, BW62.5, CR "5" (= SX1262 register value 1, their shorthand for 4/5) — confirmed from MeshCore's own `docs/faq.md`, not guessed. Will is within range of a real MeshCore repeater. **Received real, correctly-decoded packets**: a dozen-plus `[EVT] kind=lora` lines over ~1 minute, RSSI -58 to -74dBm, SNR 11.5-12.5dB — physically consistent with a strong nearby link, not noise (noise doesn't pass LoRa's header/CRC check to begin with). `stop` released cleanly mid-traffic. This is real-world confirmation of the full chain: ISR → radio task → `GetRxBufferStatus`/`ReadBuffer`/`GetPacketStatus` → RSSI/SNR conversion → `[EVT]` emission — not just a fault-free bring-up sequence against silence.
- **What this doesn't prove yet:** never explicitly read `GetDeviceErrors` for `XOSC_START_ERR`, so the TCXO's startup health at the chosen 10ms delay is inferred from the sequence not faulting (and now, from real packets decoding correctly, which needs a stable clock) rather than directly confirmed via the flag. No framing classification (meshtastic/lorawan/unknown) written yet — the payloads above are undecoded MeshCore application data, not parsed. The Cardputer's own view of this data (Sub-GHz view) doesn't exist yet — next up per Will.

---

## 2026-09-13 — lora_radio.c: SX1262 RX driver, thin in-house (D-11)

**Phase:** P3 · **By:** Will + Claude

- Wrote `firmware-c5/main/lora_radio.{c,h}` — the driver layer only (DESIGN §6.1's split), not `lora_recon.c`'s survey/framing logic, which doesn't exist yet. Thin in-house over the SX1261/2 datasheet's actual command bytes, not a lifted library — confirms D-11's leaning, since Rev D already warned generic SX126x libraries assume a bare-SX1262 board and mishandle the Wio's RF-switch/TCXO wiring.
- Reset (NRESET low 150µs, bounded BUSY wait), TCXO bring-up using D-10's resolved values, DC-DC regulator mode, RF_SW/DIO2 driven coherently for receive only, full LoRa modem config (freq/SF/BW/CR, LDRO auto-selected per the datasheet's symbol-time rule), continuous RX, and a DIO1-ISR-posts-to-queue/radio-task-does-the-SPI split (no SPI in the ISR, same rule as the DIO1 gotcha elsewhere in this codebase).
- No transmit-shaped function anywhere in the file — confirmed by `tools/check_rx_only.py`, which scanned it along with everything else and found nothing banned.
- Command byte layouts (opcodes, param encodings, IRQ bits, RF frequency formula, RSSI/SNR conversion) were transcribed directly from the official Semtech SX1261/2 datasheet (Rev 1.2), not guessed or copied from a random library — same standard as D-10.
- Caught and fixed two real issues from an actual `idf.py build`, not just editor lint: an `IRAM_ATTR` on both the DIO1 ISR's forward declaration and its definition made GCC allocate two conflicting IRAM sections (attribute belongs on the definition only), and a dead bounds check (`uint8_t len > 255` can never be true). Also caught and removed a stub `__attribute__((constructor))` function I'd left mid-file while thinking through ISR registration — real task/ISR creation now lives in `lora_radio_init()` where it belongs.
- Builds clean: `./tools/build_firmware.sh` (both firmwares) and `./tools/check_protocol.sh` (protocol/rx-only gates) both pass.
- **Not bench-validated:** no Wio harness assembled yet (P3 entry gate unmet — needs pull-ups/passives and an 862–930 MHz antenna first). Everything here is host/compile-verified only. `lora_recon.c`, the OCP verb wiring (`lora_config`/`lora_listen`/`lora_status`), and the deck's Sub-GHz view are still unwritten.

---

## 2026-09-13 — D-10 resolved: SX1262 TCXO voltage settled, delay is a verified starting point

- Read the official Semtech SX1262 datasheet (§13.3.6, `SetDIO3AsTCXOCtrl` opcode `0x97`) and Seeed's Wio-SX1262 module datasheet in full. `tcxoVoltage` is a fixed enum: `0x02` = 1.8 V, confirmed safe by both sources (Wio's documented 1.7–3.3 V TCXO range, and the chip's `VDDop > VTCXO + 200 mV` rule — 3.3 V supply leaves a 1.5 V margin at 1.8 V). `delay(23:0) × 15.625 µs` is the exact timeout formula.
- The actual startup-time value is genuinely not published anywhere Rev D cites — Semtech's datasheet says startup time is TCXO-component-specific, and Seeed never names the TCXO part. That's a real gap, not something missed.
- Closed it without guessing: start at `delay = 640` (10 ms, generous for any common 32 MHz TCXO), and let the chip confirm it via its own `XOSC_START_ERR` flag on the bench rather than asserting a number as correct. A too-short delay just raises that flag (clear with `ClearDeviceErrors`), no damage either direction. Enter `STDBY_XOSC` before RX so the delay is paid once, not per burst.
- Updated D-10 (now ✅ Decided) in `docs/DECISIONS.md`, Rev D §4, and ROADMAP P3's entry-gate note.
- **Not bench-validated:** whether 10 ms actually clears `XOSC_START_ERR` on the real board remains to be measured when the Wio harness is assembled.

---

## 2026-09-13 — Cap TFT V2 display corrects Rev D's TFT pin assumptions

- Replaced Rev D §5's placeholder "ordered 11-pin TFT with touch" with the actual selected board: MakerWorld's "Cap TFT V2" display expansion — 8-pin ILI9341, no touch, no MISO, with an onboard step-down regulator (user-identified as AMS1117-3.3) that feeds off Cardputer 5V OUT and powers the display VCC + BLK together. This also resolves §9's previously-open "final regulator is not selected" gap for the TFT.
- **Real pin assignment differs from the earlier draft:** TFT RESET lands on **GPIO3**, which Rev D's own pin table already flagged as an S3 JTAG strap pin to avoid; DC lands on GPIO6 instead of GPIO4, leaving GPIO4 unused/free. Documented as-is (the board is a fixed, already-designed PCB — there's no wiring left to choose), with an explicit bench gate: verify cold boot and JTAG both still work with GPIO3 driven as reset before calling this ready.
- Confirmed the display cap does not touch GPIO13/GPIO15 (GNSS UART) or the reserved internal-I2C pins, so §6's GNSS assignment is unaffected by sharing the same 14-pin rear connector. How the two physically share/stack on one socket is still an open assembly question.
- Also retargeted the CC1101 addition in `docs/hardware/c5-dual-radio-wiring.md` from 915 MHz to its native 433 MHz band (387–464 MHz, per the actual E07-M1101D-SMA module), since the Wio-SX1262 already owns 862–930 MHz — no GPIO changes, antenna/band references only.
- **Not bench-validated:** GPIO3-as-reset boot/JTAG behavior, actual regulator part number/current, and CC1101 433 MHz receive all remain required gates before field use.

---

## 2026-09-13 — Oscilla brand and UI system

**Phase:** P7/P8 design preparation · **By:** Will + Codex

- Added docs/brand/ as the visual/UI handoff: the reviewable HTML master,
  agent-facing README, full Lens Core SVG and its small lens/dot glyph.
- Locked Lens Core as the sole mark and OBSERVE THE NOISE. as the hero line.
  The mark is intentionally free of Signal Pink; pink is reserved for focused
  UI state and notable detections.
- Recorded the retro sci-fi mystic-observatory design philosophy: dimensional
  geometry must orient a field, relationship or reading, never become
  decorative interference.
- Established the Cardputer as the sole control surface. Its top bar owns view
  name, B/G/S/R dots and the terminal far-right battery; its transport strip
  owns LIVE/CACHED and backpack freshness.
- Established the external ILI9341 as a chrome-free display-only viewport:
  no controls or duplicated status. It uses a vertical visualization/selected
  detail split and remains optional to every workflow.
- **Not implemented or hardware-verified:** these are visual/UI contracts for
  forthcoming display work, not a claim that P5/P7/P8 exit gates are met.

---

## 2026-09-13 — proposed CC1101 dual-radio wiring addendum

- Added `docs/hardware/c5-dual-radio-wiring.md` as the proposed, bench-gated authority for adding a CC1101 to the C5 backpack, and `docs/hardware/c5-dual-radio-wiring.svg` as its visual harness reference.
- Allocation: CC1101 shares C5 SPI GPIO8/9/10; C5 GPIO7 / XIAO D3 is a dedicated CC1101 CSn with a 10 kΩ pull-up. GDO0/GDO2 remain open initially; driver polling avoids inventing a second interrupt pin.
- Preserved receive-only scope: Wio-SX1262 remains the LoRa observer, CC1101 is limited to compatible legacy OOK/FSK/GFSK receive, separate antennas, and one active sub-GHz engine at a time.
- **Not bench-validated:** continuity, MISO release, boot state, radio receive, coexistence, and 3.3 V rail margin remain required gates before field use.

# Oscilla — Worklog

> Append-only, newest first. One entry per working session or notable event: what was done, what was decided, what surprised us, what the bench showed. Keep it factual — this is the project's memory. Cross-reference `D-n` ([`docs/DECISIONS.md`](docs/DECISIONS.md)) and phases ([`ROADMAP.md`](ROADMAP.md)).

**Entry template:**
```
## YYYY-MM-DD — <short title>
**Phase:** Pn · **By:** <who>
- What happened / what changed.
- Decisions touched: D-n → …
- Surprises / open threads.
```

---

## 2026-09-12 — Rest of the P7 sniffer suite: deauth_detector, Spectrum (channel_view/packet_monitor)
**Phase:** P7 (same early/out-of-sequence batch) · **By:** Will + Claude

Finished the remaining DESIGN §7.2 items from the sniffer/Contacts work above:
`deauth_detector` and the **Spectrum** view (`channel_view` + `packet_monitor <ch>`).
Same caveat as everything else in this batch: host-verified and both
firmwares build clean, but P7's real exit gate (on hardware) isn't claimed.

- **Shared the channel-hop list.** `wifi_sniff.c`'s channel array would have
  been copy-pasted into three more engines; pulled it out to
  `wifi_channels.c` (`wifi_channels(&count)`, D-14's DFS-exclusion reasoning
  moved with it) so `wifi_sniff.c`, `wifi_deauth.c`, and `wifi_spectrum.c`
  all hop the identical list and can't drift out of sync with each other.
- **`deauth_detector`** (`wifi_deauth.c`, new): hops the shared list watching
  for 802.11 deauthentication/disassociation frames — purely passive.
  Checked this against DESIGN.md directly before building, since "deauth" is
  also listed under "explicitly out of scope": that line means *sending* a
  deauth frame (offensive, TX), while DESIGN's own defensive-capability table
  lists "deauth-detector" as explicitly Included. New `deauth_parse.c`
  (bounds-checked, same treatment as `beacon_parse.c`/`probe_parse.c` — 16
  known answers + 100k-frame ASan/UBSan fuzz). Unlike the sniffer's
  new-pairing-only events, **every** detected frame is reported: a burst is
  the signal an operator watches for, not something to deduplicate away.
- **`channel_view` / `packet_monitor <ch>`** (`wifi_spectrum.c`, new): both
  just count frames (`WIFI_PROMIS_FILTER_MASK_ALL`, no parsing at all, so
  there's no bounds risk to fuzz here) — `channel_view` hops and reports one
  live reading per channel per lap, `packet_monitor` locks to one channel
  and reports packets/s on a 1 s window. Neither has a snapshot-dump verb
  (none was ever registered in `ocp.h` for them, which is what confirmed
  this design rather than one I had to guess): the `[EVT] kind=chan` stream
  is the whole story, and it's self-healing under a dropped event — every
  channel gets a fresh reading again next cycle, unlike the sniffer's
  new-pairing events which are gone for good if dropped.
- **Deck: Spectrum card** (`model/spectrum_model`, `ui/spectrum_view`), added
  to the `,`/`/` home-card cycle alongside Link/Contacts/Info. Broad mode
  draws a live bar chart across every channel; `enter` on a selected bar
  locks onto it via `packet_monitor`, showing one big packets/sec readout —
  the same "list drills into detail" shape as Sweep→Trace, just folded into
  one screen with a `locked()` flag instead of a second Screen enum value,
  since the "detail" here is one number, not a multi-field record.
- **Verified:** `check_protocol.sh` (all suites, three more new ones —
  `deauth_parse_test` 16 assertions, `spectrum_model_test` 12 assertions,
  plus `sniff_track_test` already covering the shared classification
  patterns) and `build_firmware.sh` (both targets, clean). **Not verified:**
  any of this over real RF, or on the actual Cardputer screen.

---

## 2026-09-12 — Deauth card, ahead of a UI rework DESIGN will need
**Phase:** P7 (same early/out-of-sequence batch) · **By:** Will + Claude

Will asked for a deauth_detector card on the deck, explicitly flagging that
the card set is growing past what the current one-row-of-cards nav was
designed for and a real rework is coming — "that's a tomorrow issue." Built
it in today's pattern rather than blocking on a redesign that hasn't
happened yet:

- **`model/deauth_model` + `ui/deauth_view`**: a live, newest-first log
  (capped at 64 rows) of deauth/disassoc detections, same shape as
  `SpectrumModel` — no snapshot-dump verb exists for `deauth_detector`
  either, so the `[EVT] kind=deauth` stream is the only source of truth.
  Colour (not text) carries deauth-vs-disassoc to keep each row to one
  line: mac, reason code, RSSI.
- **Added as a 5th home card** (`,`/`/` now cycles Link → Contacts → Info →
  Spectrum → Deauth → wraps). `s` starts/stops, `;`/`.` scrolls the log.
- **Worth being honest about:** DESIGN §7.2's view table has no row for
  `deauth_detector` — Sweep/Trace/Contacts/Spectrum all map to a verb there,
  this doesn't. Given the explicit "rework later" framing, adding a 5th
  card to the existing linear cycle now and letting the eventual redesign
  reorganize all of them at once seemed better than either blocking this
  feature on a redesign or inventing a one-off navigation pattern just for
  this card. Flagged in `deck_app.h`'s screen-flow comment so this doesn't
  read as DESIGN drift nobody noticed.
- **Verified:** `check_protocol.sh` (`deauth_model_test`, 11 assertions) and
  `build_firmware.sh` (both targets, clean). Not yet seen on the actual
  display.

---

## 2026-09-12 — Deck: flicker fix, Info card, arrow-key cards; a real display bug found live
**Phase:** P7 (deck UX, same early work) · **By:** Will + Claude

- **Flicker fix.** Every view was drawing straight to `M5Cardputer.Display`: a `fillScreen()` then redraw is visible mid-SPI-transfer on every redraw, which is what read as "flashy." New `ui/canvas.h/.cpp` — a single full-screen `M5Canvas` sprite every view now draws onto — with one `pushSprite()` per frame in `DeckApp::draw()`. Compositing happens off-screen; the panel only ever sees a complete frame land atomically.
- **Navigation: home cards + drill-down.** `,`/`/` (the physical arrow-key cluster on the Cardputer keyboard, already established for `;`/`.` scrolling) now cycle **Link → Contacts → Info → wraps**. Sweep/Trace stay a drill-down from Link (`w`, then `enter`), unchanged. Removed the `c`-from-Link sniffer shortcut since Contacts is reachable directly now; starting/stopping the sniffer is `s` from within the Contacts card, so just browsing there has no side effect (doesn't grab the PHY).
- **New Info card** (`ui/info_view`): deck free heap (`ESP.getFreeHeap()`), battery % + charging state (`M5.Power`, "n/a" if the board reports no fuel gauge), deck uptime; probe free heap + uptime via a `status` poll every 2 s while the card is open, with a "Xs ago" staleness indicator since it's polled, not pushed. Needed one small protocol tidy-up: `[STATUS]`'s `heap=` key had never been given an `OCP_K_HEAP` constant in `ocp.h` (a P1-era gap) — added it and updated `ocp_server.c`'s one call site, since the deck is now a second reader of that key and literal drift is exactly what `ocp.h` exists to prevent.
- **Real hardware round-trip, including a real bug find.** Reflashed both boards (see the BOOT+RESET saga below), connected them over Grove, and Will could see Contacts populate live. He immediately spotted `1575` in the channel column — turned out to be channel **157** (one of the new UNII-3 5 GHz channels) printed flush against the band digit `"5"` with no separator (`%3u%s` → `"157"` + `"5"`). **This exact bug was already sitting in Sweep**, just never noticed because 2.4 GHz channels are 1-2 digits and don't collide as legibly. Fixed both to print `"157 5G"`. Neither host tests nor the build could ever have caught this — it's a formatting-only defect, invisible until a human looked at the actual screen. Exactly the kind of thing "verified: builds and passes host tests" doesn't cover, and why the WORKLOG entry below stopped short of claiming the UI was checked.
- **The BOOT+RESET saga, recorded because it's a real gotcha, not a one-off.** Reflashing the probe after `--gate-sniffer` hit AGENTS.md gotcha 6 for real: esptool's own RTS/DTR reset-into-bootloader toggle made the port hang (`Write timeout`, reproduced even with a bare `pyserial` write, no esptool involved) — the C5's native USB-Serial-JTAG peripheral, not an external CP210x/CH340 bridge, doesn't always tolerate the classic auto-reset dance. A plain BOOT+RESET button-press didn't clear it either, because a normal reset just re-enters the same stuck application state. What actually worked: a **full power-cycle while physically holding BOOT** (forces ROM bootloader entry independent of any running app), then `esptool --before no_reset` (skip its own toggle, since the chip is already sitting in bootloader). One clean cycle through that was enough to un-wedge `default_reset` for the actual flash. Worth remembering next time a C5 stops accepting writes: don't keep retrying the same reset flag, and don't assume BOOT+RESET alone fixes it if the hang is at the write level rather than the reset level.
- **Current hardware state:** probe on `build-uart` (Grove), deck on `cardputer-adv` with all of the above. Confirmed live over Grove by Will. Not yet re-verified: the channel/band fix just flashed above — next look at the screen should confirm `1575` is gone.

---

## 2026-09-12 — Promiscuous sniffer (P7 work, started early/out of sequence)
**Phase:** P7 (started ahead of P3-P6; see note) · **By:** Will + Claude

- **Deliberately out of roadmap order.** ROADMAP.md gates P7 ("Passive suite
  completion") behind both P2 *and* P6 (combined soak & power), and P3-P6
  haven't started. Will asked to build the sniffer now anyway. Recording this
  so the sequencing gap is visible rather than silently skipped: the P7 exit
  gate (every DESIGN §7.2 view backed by real frames, on hardware) is **not**
  claimed met by this entry, and P6's current-budget work still needs doing
  before this can run alongside the other radios with a real interlock.
- **`start_sniffer` / `show_clients` / `show_probes`** (`wifi_sniff.c`, new):
  passive promiscuous capture — plain round-robin over 2.4 GHz channels 1-13
  (`SNIFF_DWELL_MS` each; 5 GHz and D-UCB dwell weighting deferred to P8,
  D-6). Builds two in-RAM tables from received frames only, transmitting
  nothing: AP<->client pairings from data-frame address fields (ToDS/FromDS
  disambiguates STA vs. BSSID; ad-hoc/WDS frames are skipped, not
  misparsed), and mac+SSID probe-request pairings — the latter is the
  "device-tracking goldmine": a phone reveals its saved-network list via
  probe requests even while not associated to anything.
  - New parser **`probe_parse.c`**, deliberately separate from
    `beacon_parse.c`: a probe request has no 12-byte fixed field before its
    IEs (beacons/probe responses do), so reusing `beacon_info_t` would carry
    fields that don't apply. Same bounds-checked-against-attacker-bytes
    treatment: 16 known answers + 100k-frame ASan/UBSan mutation fuzz,
    wired into `check_protocol.sh` alongside the beacon test.
  - `[EVT] kind=sniff|client|probe` fire only on *new* sightings (a
    packet-count ticker and first-seen pairings), matching "events are
    lossy by design" (OCP-SPEC §5.1) — `show_clients`/`show_probes` are the
    authority, capped (128/192 rows) well under `OCP_MAX_FRAME_ROWS` so
    unlike `[SCAN]` there's no paging.
  - `ocp.h` grew three keys (`pkts`, `mac`, `ssid`) and CSV field-count
    constants for `[CLIENTS]`/`[PROBES]`; OCP-SPEC.md §10.4 documents the
    wire format and updates §8's worked example to the actual `[SNIFF]`
    compact-frame shape.
- **Deck: Contacts screen** (new `model/contacts_model`, `ui/contacts_view`,
  wired into `deck_app`). Same split as Sweep/Trace: the model trusts only
  `[CLIENTS]`/`[PROBES]` snapshots (polled every 1.5 s while the screen is
  open, alternating the two verbs since only one command is in flight at a
  time) for the shown table; `[EVT]` only updates a one-line ticker, so a
  dropped event never desyncs the list from the probe's. `Link --c-->
  Contacts`, `x` swaps the clients/probes tab, `s` starts or stops.
  - **`back()` needed a real fix, not just a new case.** Sweep/Trace's
    `` ` ``-to-stop only fires `client_.stop()` when a command is *pending*.
    `start_sniffer` isn't like `scan_networks`: it replies immediately and
    keeps streaming, so its pending flag clears the instant `[SNIFF]`
    lands — leaving the screen would otherwise never send `stop` and the
    probe would keep hopping and capturing after the deck moved on. `back()`
    now also stops unconditionally when leaving the Contacts screen.
- **Verified:** all host-side checks (`check_protocol.sh`, 15 new contacts-
  model assertions, `ocp_repl.py --selftest`); both firmwares build clean
  (`build_firmware.sh`) with the new object files confirmed present in the
  probe binary. **Not verified:** anything over the air — no live AP/client
  traffic exercised the promiscuous path, and the Contacts screen hasn't
  been seen on the actual Cardputer display. `ocp_repl.py` has no
  `--gate-sniffer` yet (mirroring `--gate-wifi` would need a live RF
  environment to assert against); flagging rather than writing one blind.

---

## 2026-09-12 — Sniffer follow-up: 5 GHz, host-testable tracking, --gate-sniffer
**Phase:** P7 (same early/out-of-sequence work as the entry below) · **By:** Will + Claude

Three gaps Will caught in the first pass:

- **5 GHz was missing.** The first cut hopped 2.4 GHz only, reasoning that
  extending the channel list was future work. Wrong call for a chip whose
  whole point is dual-band, and `inspect_network` already proves
  `esp_wifi_set_channel()` accepts a 5 GHz channel number on this exact
  hardware (P2's WORKLOG entry: a real WPA3 AP inspected on 5 GHz). Added
  the non-DFS channels — UNII-1 (36/40/44/48) and UNII-3
  (149/153/157/161/165) — to the round-robin. **Left out UNII-2/2e (DFS,
  52-140)**: `esp_wifi_set_channel` silently fails on a channel the
  configured regulatory domain doesn't permit tuning to, and on failure the
  radio stays on its previous channel while the hop index still advances —
  which would mislabel captured frames with the channel we *meant* to be
  on, not the one we're actually sitting on. DFS also carries its own
  radar-avoidance procedure that a passive listener still touches. Worth
  a real look, not a default; documented in `wifi_sniff.c`'s header comment
  and OCP-SPEC.md §10.4 rather than silently doing it.
- **The tracking logic had no test.** `wifi_sniff.c` mixed IDF calls
  (promiscuous callback registration, the arbiter, OCP framing) with the
  actual logic worth getting wrong (ToDS/FromDS address classification,
  dedup, table overflow) — which meant that logic was only ever exercised
  by "the firmware compiles," never actually run. Split it into
  **`sniff_track.c`** (pure C, no IDF dependency, same shape as
  `beacon_parse.c`/`probe_parse.c`) and made `wifi_sniff.c` thin glue over
  it. New `sniff_track_test.c`: known answers for STA-vs-AP direction from
  ToDS/FromDS, IBSS/WDS frames correctly left untracked, multicast
  rejection, dedup-in-place, table overflow at the cap, plus a 100k-round
  ASan/UBSan fuzz of both the data-frame and probe-request paths — 342
  assertions, wired into `check_protocol.sh`. This is genuinely the
  no-hardware-needed half of verification: whether the classifier is
  correct given address bytes. What it can't tell us is whether real
  frames reach the callback at all — that's still the bench's job.
- **`ocp_repl.py --gate-sniffer`**, mirroring `--gate-wifi`: drives a live
  probe through `start_sniffer`/`show_clients`/`show_probes`/`stop`,
  asserts framing and PHY-arbiter behavior (owner=wifi mid-sniff, busy on a
  second `start_sniffer`, prompt `stop` with no aborted-frame race since a
  stream has no open frame to abort), and validates `[CLIENTS]`/`[PROBES]`
  row shape when populated. Real AP<->client and probe-request rows depend
  on RF traffic actually happening during the ~12 s window the script
  listens, so those checks SKIP rather than FAIL on an empty table instead
  of asserting devices exist nearby.
- **Run for real, on the bench** — both boards are attached to the dev
  machine (probe `38:44:BE:1F:4F:A0`, deck `50:78:7D:CE:6D:64`), so this
  didn't have to wait for Will. Built `--bench` (probe talks OCP over its
  own USB-JTAG, no Grove needed), flashed with `--chip esp32c5` per gotcha
  9, ran `--gate-sniffer`: **23/23 passed**, and not just the
  structural ones — real traffic showed up inside the ~12 s window (6
  client-link events, 1 probe-request event, 11 stored client rows, 7
  probe rows) and the hop genuinely reached 5 GHz mid-run
  (`[11, 12, 13, 36, 40, 44]` in one sample), which is real confirmation
  that `esp_wifi_set_channel` accepts those channels on *this* board, not
  just an inference from `inspect_network`'s earlier 5 GHz success.
  `pkts` was non-decreasing throughout, `stop` landed in 0.2 s, PHY
  released cleanly. **The probe is currently flashed with the `--bench`
  (USB-transport) build, not the default Grove/UART one** — worth knowing
  before reaching for the Grove cable next; say the word and I'll flash it
  back to `build-uart`. The deck's Contacts screen still hasn't been seen
  on the actual display — that's a visual check only Will can do.
- Re-verified after all of the above: `check_protocol.sh` (all suites,
  including the two new ones) and `build_firmware.sh` (both targets, clean).

---

## 2026-09-12 — P2 complete: Sweep and Trace on the deck
**Phase:** P2 → P3/P4 · **By:** Will + Claude

- **P2 exit gate met.** Will drove the deck over Grove: Sweep, scroll, Trace, re-inspect, stop mid-scan (twice), back. The deck log (counts only, no SSIDs/BSSIDs): scans of **134 and 125 APs, 0 malformed rows**, in a single page each at ~10.5 s. Four inspects rendered. `stop` during the listening countdown aborted within ~0.2 s both times, and the next scan worked. Earlier on the probe: `--gate-wifi` 41 passed, 1 skipped.
- **Deck app:** `app/deck_app` owns the screen flow (Link → Sweep → Trace, `` ` `` = stop + back), auto-pages, and clears the list on a probe reset, since the probe's stored indices died with it. Views only draw. `main.cpp` is wiring again.
- **Two defects caught before they ran:**
  - Trace would have shown "listening" whenever a keepalive ping was pending, and could have shown the *previous* AP's inspect result. It now keys on a pending `inspect_network` and a matching `idx`.
  - The Grove RX buffer was 2 KB, about 180 ms of data, against a ~20 KB page burst and full-screen redraws. It's now 16 KB and drained before drawing. The 0-malformed result above is the evidence it's enough; a full 256-row page hasn't been exercised on the deck yet.
- **Open thread:** one of four deck inspects took ~5 s end to end; the others took ≤0.6 s. The probe's capture window is at most 2 s, so ~3 s is unaccounted for. It could be a sparsely beaconing AP plus deck-side timing, or a keypress landing while a keepalive was pending. Not reproduced; I'm recording it rather than guessing.
- **What P2 leaves unproven:** `mfp_required=1` on a real WPA3-only AP (none in range; host test only), and paging beyond one frame on hardware (never more than 256 APs here; host tests only).

---

## 2026-09-12 — P2: the probe scans Wi-Fi, passively
**Phase:** P2 · **By:** Will + Claude

- **Contract first** (`48983fc`), OCP-SPEC §10:
  - **Scans are passive**, because ESP-IDF's default active scan transmits probe requests and D-8 forbids that.
  - Paging via `show_scan_results <first>` with `n`/`total`/`first` on `[SCAN] BEGIN`, since the 256-row cap binds here.
  - The `[INSPECT]` shape.
  - `stop` may be sent while a command is pending; the cancelled command replies with `aborted=1` before `[STOP]`.
- **`tools/check_rx_only.py`**: the verb table rules out a transmit *verb*; this rules out a transmit-capable *driver API* behind an innocent verb. 22 APIs are banned on the probe, and every radio API on the deck. It strips comments first. Mutation-tested, which caught one hole in it: quoted `#include "esp_wifi.h"` slipped past because string stripping erased it. Fixed.
- **Beacon parser** (`beacon_parse.c`, pure C): reads over-the-air bytes any nearby transmitter controls. 22 known answers plus a 100k-frame fuzz under **ASan/UBSan** in `check_protocol.sh`. Removing each bounds check found one gap: the RSN-capabilities length check was missed, because known-answer frames lived in a 512-byte stack buffer (invisible to ASan) and the fuzzer never made an element's length disagree with the frame end. Known answers now parse exact-size heap copies, and the fuzzer corrupts element lengths; all three removals are caught. UBSan also caught a division by zero in my own fuzzer.
- **Probe:**
  - `radio_arbiter` (single PHY owner, teardown on `stop`).
  - `wifi_recon`: passive dual-band scan, 250 ms dwell, results sorted by RSSI, up to 512 stored, paged at the cap.
  - `ocp_frame` now holds a frame lock across `BEGIN..END`, so another task's reply can't split a `[SCAN]`.
  - Caps are advertised only if Wi-Fi actually initialised.
  - Unimplemented verbs now answer `unknown` rather than `nocap`.
- **On hardware:** passive scans find ~100–130 APs in ~10.5 s across both bands, channels 1–157. **`--gate-wifi` 21/21, twice**: owner=wifi mid-scan and none after, RSSI order, band/channel agreement, known auth labels, paging, badarg on page 0 and past the end, busy on a second scan, stop mid-scan giving aborted `[SCAN]` before `[STOP] running=1`. The gate prints counts only: scan results are field data and stay out of the repo and this log (SECURITY.md).
- **Found on the way:**
  - `esp_wifi_set_band_mode` returns `ESP_ERR_WIFI_NOT_STARTED` before `esp_wifi_start()`. The log only said "wifi unavailable: NOT_STARTED", so init steps are now named in errors.
  - The boot log showed **8 MB flash, but we built for 2 MB since P0**. Fixed.
  - Bytes left over from an esptool reset prefixed the first command (`unknown verb`). Tools and the deck client now send a newline first, which the spec says is ignored. That exposed a deck client bug: the junk's `[ERR]` would have cancelled a pending `hello`. Now only `[HELLO]` resolves `hello`, with a test mutation-checked.
  - P1 gate's "radio verb → nocap" broke once Wi-Fi existed. It now picks a verb the advertised caps don't cover, read from `ocp.h` (`scan_bt` today).
- **`inspect_network`** (`wifi_inspect.c`): tunes to the AP's channel and captures its beacons in promiscuous mode, parsing them with the fuzzed `beacon_parse`. The capture ends after 3 beacons or 2 s. The driver-task callback only matches, parses and records; the reply comes from a timer or `stop`, and `finish()` is idempotent so they can race.
- **On hardware, `--gate-wifi` passes 41, skips 1.** Inspect works on both bands: 3 beacons each, 102 ms interval (100 TU), reply ≤ 0.6 s, idx/bssid/ch/band match the scan row. **Security semantics cross-checked against real APs**: a WPA2/WPA3 transition network reports MFP-capable, an open network reports no RSN. **No WPA3-only AP is in range, so `mfp_required=1` is only covered by the host known-answer test, not on hardware.**
- **Three gate bugs of mine, fixed:**
  - A reply that arrived inside the `status` window made `wait_for` sit out its full timeout, reporting 6.6 s for a sub-0.6 s reply.
  - The stop-vs-inspect check was racy, because a 3-beacon capture can finish before `stop` lands. It now requires the ordering and aborted⇔running=1 to agree every time, and the abort path at least once (3/3).
  - **Skipped checks were printing `PASS`.** Reports now have a separate SKIP state, never counted as passed.
- My first two attempts at that report refactor aborted mid-script. Each validated its edits before writing, so neither touched the file. The third validated every edit up front, then applied them.
- **Deck groundwork for Sweep/Trace, all host-tested and mutation-checked:**
  - `ocp_csv`: a C++ `[SCAN]` row splitter, diffed against Python on 3,000 rows (377 malformed, rejected identically). Mirroring it found **the Unicode-strip bug again, in `split_csv_row`**: `.strip()` also removed `\x1c`/`\xa0` around fields. Now SP/HT only.
  - `ocp_client`: per-verb timeouts (scan 30 s, inspect 6 s, else 2 s). A passive scan would have died at the old 2 s. `stop` is tracked separately so it can be sent while a command is pending: the aborted frame answers the command, `[STOP]` answers the stop. 43 tests. One old test changed on purpose: it expected `scan_networks` to time out at 2 s.
  - `model/scan_model`: validates every row (7 fields, idx exactly in sequence, channel and RSSI in range, known band, 17-char BSSID) and counts rather than trusts malformed ones. Pages automatically, ignores stale pages, caps at 512 rows and shows it. 17 tests.
  - My first `absorbInspect` hand-rolled a second k=v parser, dead code included. Replaced with the tested parser before it was ever run. It now also parses `uptime_s` as 64-bit, since `long` is 32-bit on the S3.
- **Next:** the deck's Sweep/Trace views over real frames, then the P2 exit gate on the deck.

---

## 2026-09-12 — P1 complete: the deck drives the probe
**Phase:** P1 → P2 · **By:** Will + Claude

- **P1 exit gate met on both demos.** The deck firmware talks to the probe on its own over Grove, with no laptop in the link. Demo 2: Will pressed RESET on the XIAO three times mid-session. Each time the deck detected the unsolicited `[HELLO]`, stayed `ready`, counted exactly 8 boot-noise lines, and `stray=0` (no boot text taken for a frame). 51 keepalive pongs, 0 timeouts, 0 errors. Details: `docs/hardware/link-bringup.md`.
- **`ocp_client`**: handshake, one command at a time, reply/event routing, timeouts, reset detection, `Incompatible` for an unknown proto. **33 host tests** against a scripted probe, including a reset mid-command *and* mid-frame using the real ROM text, the millis() wrap, and reboot-then-`[HELLO]`. Two tests needed fixing before they meant anything: two placeholder checks that could never fail, and one that looked at output left over from earlier sends. The contract-only check (a verb not in `OCP_VERB_TABLE` never reaches the wire) was mutation-tested. It's the deck-side half of D-8.
- **Spec §5.2 addition:** a timeout on a *liveness* verb (`hello`, `ping`) means `Disconnected`; any other timeout returns to `Ready` as before. A slow scan is a slow command; a silent ping is a missing probe.
- **Deck app:** `main.cpp` does only the wiring; `ui/link_view` draws state, probe identity and counters, with probe text re-escaped for display. It reconnects every 2 s, pings every 3 s when idle, and has key commands.
- Will saw "probe reset: state invalidated" and read it as an error. It was the feature working, but the wording was bad. Now "probe rebooted - resynced".
- `check_protocol.sh` runs the whole deck stack on the host in ~5 s: conformance, fuzz, C++↔Python parser diff, client tests. The obsolete host-compile of the deck's `main.cpp` is gone, since it includes M5 now.
- **Next: P2, probe sees Wi-Fi.** Radio arbiter, `scan_networks`/`show_scan_results`/`inspect_network`, `[SCAN]` rows **paged at `OCP_MAX_FRAME_ROWS`**, and the deck's Sweep/Trace views.

---

## 2026-09-12 — ocp_fuzz.py, and three parser bugs
**Phase:** P1 · **By:** Will + Claude

- **`tools/ocp_fuzz.py`**: a property fuzzer for the reference parser. It checks that no input raises, results don't depend on how bytes are chunked, valid items survive boot text and log noise exactly, the parser recovers after garbage via `[HELLO]` or a timeout, line buffer and frame rows stay bounded, and escapes round-trip. 125,000 cases pass across five seeds. `--emit-corpus` writes byte streams plus the reference parser's output, so the deck's C++ parser can be diffed against it later.
- **Three real bugs, all in the reference parser the deck will copy:**
  1. **Newline injection in `encode_value`.** Python's `$` also matches just before a trailing newline, so a value ending in `\n` counted as bare and went out unquoted, splitting a frame. The fuzzer caught it by comparing contents, not just item counts. All three regexes now use `fullmatch`. The C encoder was always correct; the C/Python cross-check only compared *field* encoding, so it now covers value encoding too. With the old regex put back, it fails on 4 of 789 payloads.
  2. **Unbounded frame rows.** A stray `BEGIN` followed by endless rows grew memory forever, fatal on a Cardputer without PSRAM. New contract limit `OCP_MAX_FRAME_ROWS 256`: an over-limit frame is dropped whole, never delivered truncated, so larger results must be paged. **This binds P2's `[SCAN]`.**
  3. **A late bare `[TAG] END` became an empty compact frame.** After a timeout, a late `END` would have shown up as "0 results". Spec: a compact frame needs at least one token before `END`, and a bare `END` with no open frame is noise.
- **I discarded uncommitted work and recovered it.** While proving the checks catch regressions, I undid a deliberate break with `git checkout -- tools/ocp.py`. That reverted to the last commit and threw away all three fixes. A backup taken minutes earlier held all of them; every marker was verified before restoring, and all checks passed afterwards. The later regression runs used a copy and a checksum. Rule added to AGENTS.md §6.
- **That test also found a gap:** `check_protocol.sh` passed with the bare-`END` fix removed. The fuzzer tests robustness, not spec rules, and the spec checklist (`--selftest`) wasn't in the script. It is now, and removing any of the three fixes fails it.
- **Reference parser made byte-exact** before the C++ mirror copies it. It had followed Python string rules instead of the spec: `str.strip()` also removes Unicode whitespace (`\x0c`, `\x1c`, `\xa0`…), and lines went through a lossy UTF-8 decode that re-encoded raw bytes. It now maps bytes 1:1, treats only SP/HT as whitespace, and copies values byte-for-byte. OCP-SPEC §2 now says exactly that. Canonical corpus output is all hex.
- **Bug #4: a malformed escape in a quoted `k=v` made the parser raise.** Found by reading the code while mirroring it; the fuzzer never produced that shape. Escapes are now checked before the line touches parser state, so a malformed `[HELLO]` can't abandon a good open frame, and the line becomes noise (spec §6 + checklist). The fuzzer now generates bad-escape `k=v` on every line type plus the non-SP/HT whitespace bytes. With the bug put back on a copy, 3 properties fail. The fuzz runner now reports an exception in any property as a failure instead of crashing. Self-test is at 26 checks.
- **Bug #5, spec divergence:** `decode_field` used `int(hh, 16)`, which also accepts `\x+f` and `\x f`. The C decoder and the spec reject both. Now strict.
- **Deck parser in C++** (`firmware-cardputer/src/ocp/`, plain C++17, no Arduino headers). It mirrors the reference step by step and reuses the contract's `ocp_text.c`. `tools/check_deck_parser.py` builds it on the host and diffs it against `ocp.py` on the fuzz corpus, item for item, noise included, with byte-by-byte chunking and caps checked too. First run: identical on 300 streams.
- **I didn't trust that pass, so I broke the C++ five ways.** Four breaks were caught. **Removing the row cap was missed:** no generated stream ever exceeded 256 rows, so the Python fuzzer couldn't have caught a missing cap either. The generators now produce frames at, just under and over the cap, and removing the cap is caught in both languages.
- **P0 config bug found:** `build_flags_cxx` isn't a PlatformIO option. It was silently ignored, and with `build_unflags` removing the core's flag, the deck compiled with **no `-std` flag at all** (GCC 8.4's default, gnu++14). It surfaced as `std::string_view` errors. Fixed with `cxx17.py`, which appends `-std=gnu++17` to C++ sources only.
- **Next:** the deck transport layer in C++. Testable on the host, and diffed against the fuzz corpus.

---

## 2026-09-12 — D-12 resolved: Cardputer ADV is supported
**Phase:** P1 · **By:** Will + Claude

- **D-12 → ✅.** M5Unified 0.2.21, M5GFX 0.2.28 and M5Cardputer 1.1.1 all support the ADV. Checked in source first, then on the hardware with `bench/adv_check.cpp`: the board is detected as `board_M5CardputerADV`, the 240×135 display renders correctly, and the TCA8418 keyboard shows up at `0x34` on the internal I²C bus. 52 key events captured, covering every modifier and `` ` ``. Details: `docs/hardware/cardputer-adv.md`.
- **The finding that matters: M5Unified's "Port A" external I²C uses GPIO1/2, our Grove UART pins.** In the source, that bus is only started by `external_rtc`/`external_imu` (off by default) or by including an M5 display-unit header. On the hardware, `--gate` passed 18/18 on three runs through firmware running M5Unified, the display and keyboard polling. Those rules are now AGENTS.md gotcha 13.
- **A check I got wrong.** My firmware printed `ex_i2c_enabled=1 (must be 0)`. `isEnabled()` only means a port was *assigned*, not started, so the check measured the wrong thing. Read the source to see why, and relied on the gate result for the answer. The label is fixed in the firmware.
- The keyboard library translates keys inconsistently: Ctrl+g gives `G`, Fn/Opt/Alt+letter give lowercase. The deck should read the modifier flags. Gotcha 14.
- The libraries are pinned exactly in the deck's base build, and the `board` line is no longer a placeholder.
- **Next:** P1's remaining work is unblocked: the deck's transport layer and connection state machine (demo 2), and `tools/ocp_fuzz.py`. D-10 blocks only P3.

---

## 2026-09-12 — Grove link proven
**Phase:** P1 · **By:** Will + Claude

- **The Grove UART works.** Will soldered headers on the XIAO and wired Grove (red insulated). The Cardputer runs `grove-bridge`, a USB↔Grove passthrough, and the probe runs the UART build. `ocp_repl.py --gate` from the laptop through the real cable: **18/18, four runs in a row.** `[STATUS]` shows `link=uart0`. During a reset the wire carries ROM text and then `[HELLO]`, and the parser treats that text as noise. That's exit-gate demo 1 on the real link. Details: `docs/hardware/link-bringup.md`.
- Probe app and bootloader logs moved to the C5's native USB, because Rev D §3 says keep debug text out of the command channel and our config had put it there. A reset now puts 9 lines of text on the wire, down from 59 before the move.
- Cardputer: `grove-bridge` is a separate PlatformIO build (`bench/`), so the deck app is untouched. Its flash was confirmed byte-identical to that build. Set `ARDUINO_USB_CDC_ON_BOOT=1`; without it the StampS3 board sends `Serial` to UART0 pins, not USB.

### Getting there

- **After soldering, the C5 wouldn't boot.** The LED was dark. Over JTAG the CPU was looping in ROM at `0x4003B10E` and our app never started. The strap register decoded to normal flash boot. Rewriting all three images over JTAG (verified) didn't fix it; a hardware RESET did. **Root cause still unknown**: the flash was rewritten before that RESET, so bad flash and a boot-time condition can't be told apart. Logged as open in link-bringup.md.
- **Two red herrings, both mine.** OpenOCD's `reset run` is a CPU reset that always parks this chip in ROM, so my post-reset observations looked like the fault persisting. And the only ROM symbols installed are rev0, while this chip is `eco2`, so the function names addr2line gave me were meaningless. I didn't act on either, but they cost time. AGENTS.md gotcha 10.
- **Getting OpenOCD access took two tries.** Espressif's udev rules use a `plugdev` group that Arch doesn't have, so udev silently dropped every line naming it. Changing it to `uucp` worked. AGENTS.md gotcha 11.
- **The wire was swapped** (TX→TX). Reading the C5's pin levels over JTAG while flooding zero bytes showed the signal wasn't reaching either UART pin. Will spotted the swap at the same moment. After fixing it, GPIO12 read low in 32 of 40 samples. The two outputs fought each other for a while; Rev D's 470 Ω series resistors would limit that. AGENTS.md gotcha 12.
- The LED paid for itself on its first day: "dark" narrowed a wedge to "the app isn't running" before any tool was attached.

- **Next:** P1 still needs `tools/ocp_fuzz.py`, plus the deck transport layer and connection state machine for demo 2, which D-12 blocks. The Cardputer is attached, so D-12 can be settled now.

---

## 2026-09-12 — Status LED heartbeat
**Phase:** P1 · **By:** Will + Claude

- Will asked for a status light so a working probe is visible at a glance on the bench and in the field. `status_led.{h,c}`: rapid blink while booting, 40 ms flash every 2 s when healthy, extra flash per command, solid on for a fault. **Dark means dead or wedged.**
- **The heartbeat only runs while the dispatch loop checks in.** A heartbeat on its own timer would have kept blinking through this morning's wedge. Now a stalled command loop goes dark within a second.
- **Pin: GPIO27**, taken from Seeed's XIAO ESP32-C5 pin map and recorded in Rev D §8.1 before any code used it. The web-page summary said GPIO27 wasn't a strapping pin; IDF's own GPIO reference for the C5 says it is (2, 7, 25, 27, 28). It's safe because strapping pins are only read at reset and Seeed's circuit sets the level, but it's documented the same way GPIO25 is.
- **Polarity confirmed on the board by Will**: active-low, as Seeed's example code suggested.
- Battery: 2% duty cycle. Kconfig can disable it or set the period (500 ms–10 s).
- Flashed with no BOOT+RESET, since the running probe now enters download mode from software. That confirms the yield fix. `--gate` still 18/18.
- Housekeeping: DESIGN §6.1 module map now lists `ocp_transport`, `ocp_frame` and `status_led`. The first two should have gone in with the P1 commit; AGENTS.md §7.2 says so and I skipped it.

---

## 2026-09-12 — P1 probe: OCP server on real hardware
**Phase:** P1 · **By:** Will + Claude

- Both boards on USB, no Grove cable. C5 probe USB serial `38:44:BE:1F:4F:A0`, Cardputer `50:78:7D:CE:6D:64`.
- **Probe firmware, modular:** `ocp_transport` (Grove UART0 or USB Serial/JTAG, picked in Kconfig), `ocp_frame` (writes whole lines under a lock), `ocp_server` (reads lines, dispatches verbs from `OCP_VERB_TABLE`, the six system verbs), `main`. The escaping code moved to `protocol/ocp_text.{h,c}` so both firmwares share it; checked byte-for-byte against `ocp.py` on 784 payloads, every byte value included.
- **Exit-gate demo 1 met over USB.** `ocp_repl.py --gate` passed 18/18, three runs in a row, and again when started 0.5 s after a reboot. That covers `unknown`/`nocap`/`badarg`, recovery from an over-long line, and a reboot producing an unsolicited `[HELLO]` with 59 lines of real ROM boot text read as noise, never as a frame. Not yet repeated over the Grove UART.

### What went wrong

- **I got the probe stuck, and wrote the warning for it earlier the same day.** The read loop assumed the transport's timeout would make it wait. It spun at priority 10 on a single-core chip, so the USB driver task never ran: the device stayed connected but stopped accepting writes and couldn't be flashed. Recovery needed BOOT+RESET. Fixed by yielding explicitly when idle. `CONFIG_ESP_TASK_WDT_PANIC=y` now makes a starved core reboot; by default the watchdog only prints a warning. AGENTS.md gotcha 6 reworded to "never spin without yielding".
- **The boards swapped port numbers on replug.** A C5 image went to `ttyACM1`, which had become the Cardputer. esptool refused because `--chip esp32c5` was explicit, so nothing was written. Read-only check afterwards: the Cardputer's flash doesn't match our deck build, so its original firmware is intact. From now on boards are addressed by `/dev/serial/by-id/`. AGENTS.md gotcha 9.
- After flashing from download mode over USB, `--after hard_reset` doesn't leave the ROM loader. `--after watchdog_reset` does.
- **My own build script had a trap.** Once `sdkconfig` exists, `idf.py` ignores the defaults files, so a normal build after `--bench` quietly kept the USB transport. Each variant now has its own `build-uart/` or `build-bench/` directory, and the script checks which transport ended up in the config.
- `check_protocol.sh` was still host-compiling the probe's `main.c`, which now needs IDF headers; step removed, since the real board build covers it.
- The first gate run failed one check because it started before the app was up. The gate now retries `hello` for up to 6 s. A deck will have the same race after every probe reset.

- **Next:** status LED on the XIAO (Will asked), then `ocp_fuzz.py`. Demo 2 needs the Grove cable. The deck half still waits on D-12.

---

## 2026-09-12 — Agent and security docs
**Phase:** P0 → P1 · **By:** Will + Claude

- Added [`AGENTS.md`](AGENTS.md), [`CLAUDE.md`](CLAUDE.md), [`SECURITY.md`](SECURITY.md).
- **One source of truth for agent rules.** All content is in `AGENTS.md` (tool-neutral, so any coding agent works from the same rules); `CLAUDE.md` is an 18-line pointer. Two files repeating each other would drift — the exact failure this project designs against everywhere else.
- **`AGENTS.md` is rules of engagement, not architecture.** Authority table (Rev D wins on hardware, DESIGN on software, `ocp.h` on literals, OCP-SPEC on wire behaviour), the invariants an agent could plausibly break by accident, the commands, and the nine gotchas that have already cost time — PATH shadowing, the two `.gitignore` traps, `[HELLO]` precedence, hostile decoded bytes, boot noise, single-core priority, the shared SPI bus, ISR discipline, bench power. Deliberately no architecture recap.
- **`SECURITY.md` is posture-first, disclosure-second.** States the receive-only guarantee with both honest qualifications (TX-capable silicon; receiving is not the same as being invisible), tabulates the five mechanisms that enforce it, and gives the threat model — attacker-controlled text, bounded queues, the probe/deck boundary — plus an explicit *what is not defended against* section (physical access, a lying probe, the unauthenticated Grove wire).
- **Kept it honest about status.** A security doc for a project at P0 could easily describe P1–P7 protections in the present tense. Added an implementation-status section saying exactly what is enforced today (protocol layer and host tools) versus not yet written, with a note that the file gets updated as phases land, not before.
- **Data handling is the section that matters most for a public repo.** Field logs are the operator's movement history; the gitignore is a safety net, not permission to be careless. Reporters are asked to strip positions before sending a capture — `--replay` takes synthetic input, so a protocol bug almost never needs real coordinates.
- Disclosure via GitHub private advisories or <info@d3mo.us>; repo stays public through bring-up (Will confirmed both).
- Verified every factual claim in the new docs against the tree rather than from memory: tripwire flag names, the 29-verb count, that the gitignore actually covers each extension named, and that every cross-referenced path exists.

---

## 2026-09-12 — P0 exit gate met; toolchains found; .gitignore hardened
**Phase:** P0 → P1 · **By:** Will + Claude

- **Both toolchains were already installed** — the earlier "not available" finding was wrong, they simply weren't on `PATH`. ESP-IDF **v5.5.1** at `~/esp/esp-idf` (needs its own `export.sh`), PlatformIO **Core 6.2.0** inside the VS Code extension's virtualenv at `~/.platformio/penv/bin`. Nothing needed installing.
- **P0 exit gate is now fully met.** Real board builds:
  - probe → `oscilla-c5.bin`, target **esp32c5**, 209 KB, 80% of the app partition free;
  - deck → `firmware.bin`, target **esp32s3**, RAM 5.6%, flash 7.9%.
- Added `tools/env.sh` (sources IDF, adds pio) and `tools/build_firmware.sh` (builds both). Together with `tools/check_protocol.sh` the whole gate is two commands.
- **PATH ordering trap, recorded because it will bite again.** PlatformIO's `penv/bin` is a virtualenv containing its own `python`. Prepending it to `PATH` shadows the interpreter ESP-IDF's `export.sh` just installed, and `idf.py` then dies with `No module named 'esp_idf_monitor'` — which reads like a broken IDF install and isn't. `env.sh` *appends* it; `pio`'s shebang is absolute, so it does not need to be found first.
- **D-12 is NOT resolved by this.** The deck built against `board = m5stack-stamps3` with no M5Unified dependency. An Arduino skeleton compiling says nothing about Cardputer ADV keyboard-matrix or EXT-header support. Still blocking P1.

### .gitignore — field data is the real hazard

- Rewrote it around what this project actually produces. Build output is only noise (209 MB of it); the sensitive class is **field data**: a wardrive CSV is the operator's movement history, a KML is that history on a map, an NMEA capture is raw fixes, and a coredump embeds buffered observations. All now ignored by extension, with the reasoning in the file so the next person doesn't "tidy up" the rule. Synthetic protocol fixtures are explicitly re-included.
- Two git gotchas, both caught by testing the rules rather than reading them:
  1. **`#` is only a comment at the start of a line.** `firmware-c5/sdkconfig  # generated` matched a literal filename with spaces and a hash in it, so `sdkconfig` was *not* ignored.
  2. **A file cannot be re-included if its parent directory is excluded.** `.vscode/` + `!.vscode/extensions.json` silently keeps ignoring the file; it needs `.vscode/*`.
  Verified with `git check-ignore -v` against a list of representative paths, including ones that don't exist yet (`session.kml`, `secrets.h`, `wardrive-*.csv`).
- **Next:** P1 — C5 OCP server on the Grove UART, deck transport layer, `tools/ocp_fuzz.py`. The probe half and the fuzzer are unblocked; the deck half waits on D-12.

---

## 2026-09-12 — P0 scaffold: the contract exists
**Phase:** P0 · **By:** Will + Claude

- Built the remaining P0 artifacts: monorepo skeleton, `protocol/ocp.h` v1, `protocol/OCP-SPEC.md`, `tools/ocp_repl.py`, `NOTICE`. Plus a runnable exit gate (`tools/check_protocol.sh`), an adversarial fixture, and a host Arduino stub.
- **`ocp.h` shape.** Plain `#define` literals (readable, greppable, C and C++ alike) plus an `OCP_VERB_TABLE(X)` X-macro whose rows reference those macros — so each verb string exists exactly once, and the probe's dispatch table, the tooling's help, and the §8 audit all derive from one list. 29 verbs registered.
- **D-8 is now enforced by the build, not just by intent.** `ocp.h` `#error`s if `OSCILLA_*_TX` is defined, and `test_ocp_header.c` fails if any registered verb matches a transmit-shaped name. Both run in `check_protocol.sh`. The P3 exit gate's "grep the command table for TX verbs" is no longer a manual step.

### Three gaps in DESIGN §5 that implementation forced us to close

1. **Compact vs. block frames.** §5.2 says every frame is `BEGIN`/rows/`END`, but the §5.3 `[HELLO]` example is a *single line* ending in `END`. Both forms are now specified, and a parser must accept either for any tag — so a producer can grow rows later without bumping `proto`.
2. **Escaping was unspecified.** §5.1 requires quoting but never said how. SSIDs are attacker-controlled bytes; an unescaped newline inside one would desynchronise the reader. Specified: quote, `\\` and `\"`, and `\xHH` for every byte outside printable ASCII — reversible byte-exactly. Added the corollary that decoded bytes are *still* hostile and every renderer must re-escape before display (a crafted SSID can otherwise inject ANSI escapes into the operator's terminal). `ocp_repl.py` does this; the deck's views will owe the same.
3. **`[HELLO]` outranks parser state.** Found while building the fixture: a probe that resets *mid-frame* emits boot text and then `[HELLO]` while the deck has a block frame open. A reader that only accepts the open frame's own tag swallows the `[HELLO]` and waits forever for an `END` that is never coming — the exact P1 failure, sitting in the first parser we wrote. Fixed and specified: `[HELLO]`, `[EVT]` and `[ERR]` are the three markers accepted in any state.

- Also corrected the bare-value charset: `caps=wifi24,wifi5` proves commas are legal unquoted, which the first draft's charset forbade.
- **`tools/ocp.py`** is the reference parser (the deck's C++ transport layer will mirror it); `ocp_repl.py` is a thin CLI over it. `--selftest` asserts the spec's §9 conformance checklist (21 checks, all passing); `--replay` parses a canned stream. Neither needs pyserial or hardware — only the live serial mode does. `ocp.py` also re-reads `ocp.h` and fails if its mirrored literals have drifted.
- **Exit gate is only partially verified.** `ocp.h` compiles `-Werror`-clean under gcc c99/c11/c17 and g++ c++11/c++17; the fixture round-trips. But **neither ESP-IDF nor PlatformIO is installed on this machine**, so no board build has been run. Both `main.c` and `main.cpp` compile against `ocp.h` with a host compiler, which proves contract usage but is not the same thing. P0 stays 🟡 until `idf.py build` and `pio run` are run somewhere they exist.
- **Surprises / open threads.**
  - Writing the adversarial fixture *first* is what surfaced the `[HELLO]` bug. Worth repeating for the deck parser.
  - `platformio.ini`'s `board` is the original Cardputer's `m5stack-stamps3` — a placeholder, and a live instance of **D-12**. Nothing in P0 tests it.
  - `sdkconfig.defaults` deliberately leaves the ROM console enabled on UART0. Boot noise on the control line is unavoidable anyway (Rev D §3) and the protocol is specified to tolerate it; silencing it would cost bring-up visibility and buy nothing.
- **Next:** P1 — C5 OCP server on the Grove UART, deck transport layer, `tools/ocp_fuzz.py`. Blocked on D-12 for the deck half; the probe half and `ocp_fuzz.py` are not.

---

## 2026-09-12 — Revert to receive-only (D-8)
**Phase:** P0 · **By:** Will + Claude

- Will decided to **drop transmit entirely and fall back to receive-only on every radio**, LoRa included.
- Context: earlier in the day we'd designed a radio-agnostic transmit gate (see entry below) to keep TX reachable long-term. On reflection Will preferred the simpler, stronger posture. (Note for the record: the gate design was not blocked by any safeguard — TX was fully implementable; this was a scope choice.)
- Changes:
  - **DESIGN §8** rewritten from "transmit gate" back to "receive-only boundary" — no TX verb compiled into any build → no reachable firmware path to transmission. Kept the honest caveat that the SX1262 is TX-capable silicon, so the guarantee is about *code paths*.
  - Removed from the design: `lora_tx_arm`/`lora_tx` verbs, `OSCILLA_*_TX` flags, `lora_tx` cap, `[ERR] code=nogate`, TX operator-obligation text, per-radio gate section.
  - `lora_radio.c` / `lora_recon.c` scoped to RX path only.
  - **D-8** → receive-only (restores the v0.1 structural stance). **D-9** (LoRa region) → closed/not-applicable, drops off the critical path. **D-13** reframed as "any transmit, if ever" (post-v1, preserved by architecture not by code).
  - ROADMAP **P3** retitled "LoRa (RX)"; TX work items removed; exit gate now includes grepping the built command table for zero TX verbs.
- **Net effect on blockers:** only D-10 (TCXO delay, needed even for RX init) and D-12 (ADV board support) remain blocking. LoRa region no longer matters.
- **Next unchanged:** P0 remaining — monorepo skeleton, `protocol/ocp.h` v1, `OCP-SPEC.md`, `tools/ocp_repl.py`, NOTICE.

---

## 2026-09-12 — Reconciliation & doc scaffold
**Phase:** P0 · **By:** Will + Claude

- Read both source docs. Found DESIGN.md v0.1 and backpack Rev D described materially different machines. Reconciled DESIGN.md → **v0.2** against Rev D (the hardware authority).
- Key architecture changes captured in v0.2:
  - **GNSS and microSD move to the deck.** Probe has no filesystem; observations stream as `[EVT]`, deck geotags them against its own fix + fix age. Removed `gps.c`, probe-side SD, and `list_dir`/`send_file` verbs.
  - **LoRa (Wio-SX1262) added** as a probe radio on its own SPI bus — absent from v0.1 entirely.
  - **Arbiter is now two-lane:** internal PHY (Wi-Fi/BLE/154, one owner) + independent LoRa lane, joined by a power interlock.
  - **Control UART is C5 GPIO11/12** (UART0 pins) — boot noise unavoidable, so parser resync is load-bearing. Settles D-3, D-5 by hardware.
  - **Bulk file transfer deferred** (D-2) — no probe SD.
- **Transmit posture (D-8) — decided mid-session.** Will asked to keep TX reachable for *both* LoRa and the survey radios, to avoid foreclosing a future move beyond recon. Replaced v0.1's "no code path to transmission" structural claim with a **uniform, radio-agnostic transmit gate**: compile-gated + runtime-gated per radio, off in every default build. v1 implements only LoRa TX; survey-radio TX flags (`OSCILLA_WIFI_TX` etc.) exist as named seams with no code behind them. Documented honestly that §8 is now a *policy* boundary, not a structural guarantee.
- Framework decided: **M5Unified + PlatformIO** (D-1), with OCP client/model kept framework-agnostic.
- Created ROADMAP.md (P0–P8 with entry/exit gates), docs/DECISIONS.md (D-1…D-13), this worklog.
- **Blocking threads flagged:** D-9 (LoRa region — needed before TX), D-10 (TCXO startup delay — don't guess), D-12 (Cardputer ADV support in M5Unified — verify before P1).
- **Next:** P0 remaining — monorepo skeleton, `protocol/ocp.h` v1, `OCP-SPEC.md`, `tools/ocp_repl.py`, NOTICE.
