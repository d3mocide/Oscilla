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
