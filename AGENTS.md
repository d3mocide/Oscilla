# AGENTS.md — rules of engagement for coding agents

Read this before touching the repo. It is the operating manual, not the design:
it says **how to work here**, where authority lives, and which mistakes this
project has already made so you don't repeat them.

It deliberately does **not** restate the architecture. Duplicated prose drifts,
and this project is built around not letting that happen.

---

## 1. Orientation

Oscilla is **two machines with one contract**. A Cardputer ADV (the *deck*,
ESP32-S3) carries the keyboard, screens, GNSS and storage. A XIAO ESP32-C5 (the
*probe*) carries the radios. They speak one line protocol over a 4-pin Grove
cable. Both firmwares compile against the same `protocol/ocp.h`, so the contract
cannot silently drift.

**Every radio is receive-only.** See §3.

## 2. Authority — who wins when documents disagree

| Question | Authority | Never |
|---|---|---|
| Pins, rails, bus sharing, electrical limits | [`Research/c5-backpack-design.md`](Research/c5-backpack-design.md) **Rev D** | Write a pin constant from any other document, including DESIGN.md |
| Software architecture, module structure, feature scope | [`DESIGN.md`](DESIGN.md) | — |
| Protocol literals — verbs, markers, caps, error codes, limits | [`protocol/ocp.h`](protocol/ocp.h) | Hardcode a verb or marker string anywhere else |
| Protocol wire behaviour — framing, escaping, parser duties | [`protocol/OCP-SPEC.md`](protocol/OCP-SPEC.md) | — |
| Anything contested or deferred | [`docs/DECISIONS.md`](docs/DECISIONS.md) (`D-n`) | Resolve a `⛔` decision by guessing |
| What to work on next, and what "done" means | [`ROADMAP.md`](ROADMAP.md) | Declare an exit gate met without evidence |
| What actually happened | [`WORKLOG.md`](WORKLOG.md) | — |

Currently blocking: **D-10** (SX1262 TCXO startup delay — Rev D says *do not
guess*). D-12 (Cardputer ADV support) is resolved.

## 3. Invariants — do not break these

1. **No transmit verb. Ever.** Oscilla v1 is receive-only on every radio
   ([D-8](docs/DECISIONS.md), [DESIGN §8](DESIGN.md)). The probe's command table
   is the entire capability surface; with no TX verb in it, there is no
   reachable firmware path to transmission. `ocp.h` `#error`s on any
   `OSCILLA_*_TX` flag and `protocol/test_ocp_header.c` fails on any
   transmit-shaped verb name, and `tools/check_rx_only.py` fails if firmware
   source reaches a transmit-capable driver API (e.g. `WIFI_SCAN_TYPE_ACTIVE`
   — **scans are passive**). **If a task appears to require transmitting,
   stop and ask** — do not add the verb, do not remove the tripwire.
2. **Pin constants come from Rev D only.** Not from DESIGN.md's summary tables,
   which exist for orientation and say so.
3. **`ocp.h` owns every protocol literal.** Changing it means `firmware-c5`,
   `firmware-cardputer` and `tools/ocp.py` must all still agree —
   `check_protocol.sh` and `ocp_repl.py --selftest` enforce this.
4. **Lifted third-party code stays isolated** in `firmware-c5/components/`,
   keeps its original copyright headers, and gets added to
   [`NOTICE`](NOTICE) **in the same session it lands** — not later.
5. **Never commit field data.** See [`SECURITY.md`](SECURITY.md).

## 4. Commands

```sh
source tools/env.sh            # both toolchains onto PATH (see §5, gotcha 1)
./tools/check_protocol.sh      # host-only: header compiles, invariants, no TX verb
python3 tools/ocp_repl.py --selftest    # OCP-SPEC §9 conformance (21 checks)
./tools/build_firmware.sh      # real board builds: esp32c5 + esp32s3
```

Installed here: **ESP-IDF v5.5.1** (`~/esp/esp-idf`), **PlatformIO Core 6.2.0**
(`~/.platformio/penv`). Nothing needs installing.

Run the checks before claiming something works. "It should compile" is not a
result; this repo's culture is that a claim without evidence is a defect.

## 5. Gotchas — every one of these has already cost time

1. **PlatformIO's venv shadows ESP-IDF's python.** `~/.platformio/penv/bin`
   contains its own `python`. Prepend it to `PATH` and `idf.py` dies with
   `No module named 'esp_idf_monitor'`, which reads like a broken IDF install
   and isn't. `tools/env.sh` **appends** it; `pio`'s shebang is absolute so it
   needs no PATH priority. Don't "fix" this by reordering.
2. **`.gitignore` has two traps.** `#` only starts a comment at the *start of a
   line* — a trailing one becomes part of the pattern. And a file cannot be
   re-included if its parent *directory* is excluded (`.vscode/` +
   `!.vscode/extensions.json` silently fails; use `.vscode/*`). Verify rules
   with `git check-ignore -v <path>`, including paths that don't exist yet.
   Do not eyeball them.
3. **`[HELLO]` outranks parser state.** A probe can reset *mid-frame*. A reader
   that only accepts the open frame's tag swallows the `[HELLO]` and waits
   forever for an `END` that never comes. `[HELLO]`, `[EVT]` and `[ERR]` are
   accepted in any state (OCP-SPEC §4.1).
4. **Decoded bytes stay hostile.** Escaping protects the transport, not the
   consumer. SSIDs and device names are whatever an attacker broadcast — they
   can carry control characters and ANSI escapes. Every renderer re-escapes
   before display (OCP-SPEC §6).
5. **Boot noise on the control line is normal traffic**, not an error. C5
   GPIO11 is UART0's default pin, so ROM chatter appears on the Grove link on
   every probe reset. A parser that wedges on it is a defect
   (OCP-SPEC §1.1). The ROM console is deliberately left enabled.
6. **The C5 is single-core — never spin without yielding.** The dispatch task
   outranks engines so `stop` lands (DESIGN §6.4), which means a loop in it
   that doesn't block starves *everything*, including the USB driver. The
   probe then stays enumerated but stops accepting writes, and cannot be
   reflashed without BOOT+RESET. Never trust a read timeout to block; yield
   explicitly when idle.
7. **The deck's SD and external TFT share one SPI bus.** One lock, SD brought
   up first, TFT write-only, no task bypasses it (Rev D §5.2). This is the
   deck's main source of hard-to-debug failures.
8. **No SPI inside the DIO1 ISR.** Post to the radio task. All hardware waits
   are bounded — fault, never hang (Rev D §4.3).
9. **`/dev/ttyACM*` numbering is not stable.** Both boards enumerate as the
   same Espressif VID/PID and swap port numbers on replug. Address them by
   `/dev/serial/by-id/…<usb-serial>` and **always pass `--chip`** to esptool —
   an explicit `--chip esp32c5` is what stopped a probe image being written to
   the Cardputer. Probe `38:44:BE:1F:4F:A0`, deck `50:78:7D:CE:6D:64`.
10. **A JTAG `reset` leaves the C5 parked in ROM.** OpenOCD's `reset run`
    is a CPU reset; the chip loops at `0x4003B10E` and never boots the app.
    Use the RESET button or `esptool --after watchdog_reset`. Halt/resume
    without reset is safe. Installed ROM symbols are rev0 and this chip is
    `eco2`, so don't trust addr2line on ROM addresses.
11. **OpenOCD needs a udev rule, and Espressif's assumes `plugdev`.** Arch
    has no such group, so udev drops every line naming it. Install
    `60-openocd.rules` with `GROUP="uucp"`, check it with `udevadm verify`.
12. **Grove is a crossover: G2 (TX) → D7, G1 (RX) → D6.** Swapped, both
    directions go silent and two outputs fight. Check with the JTAG line
    test in `docs/hardware/link-bringup.md`.
13. **M5Unified's "Port A" I²C *is* our Grove UART (GPIO1/2).** On the deck,
    never set `external_rtc`/`external_imu` and never include an M5
    display-unit header — any of these starts I²C on the link. Call
    `Serial1.begin()` after `M5.begin()`. `M5.Ex_I2C.isEnabled()` returning 1
    only means a port was *assigned*, not started; don't read it as a conflict.
14. **Cardputer keyboard: read the modifier flags, not the translated
    character.** M5Cardputer's translation is inconsistent (Ctrl+g → `G`, but
    Fn/Opt/Alt+letter → plain lowercase).
15. **Bench power rule:** both boards on their own USB, Grove 5 V (red)
   disconnected and insulated. Grove-powered operation is not evaluated until
   P6 produces a *measured* current budget.

## 6. Workflow

- **Write the adversarial test first.** The `[HELLO]` bug in gotcha 3 was found
  by building a hostile fixture before trusting the parser. Do that again for
  the deck's transport layer.
- **Append a [`WORKLOG.md`](WORKLOG.md) entry** for any real work: what changed,
  what was decided, what surprised you. Newest first. It is the project's
  memory and it is honest about mistakes — keep it that way.
- **Update [`docs/DECISIONS.md`](docs/DECISIONS.md)** when a `D-n` moves, and
  note the move in the worklog.
- **A phase is done when its exit gate is demonstrated**, on hardware where the
  gate says hardware. If you can only verify part of it, say which part and
  mark the rest unverified. Never round up.
- **Surface blockers, don't code around them.** An unresolved `⛔` decision
  means stop and ask.
- **Never `git checkout -- <file>` or `git restore` to undo a temporary
  edit.** Either one reverts to the last *commit*, discarding every
  uncommitted change in that file, not just your test edit. To break
  something on purpose, copy the file first and restore from the copy.
- **Prove a check catches the bug.** After fixing something, put the bug back
  (on a copy) and confirm the check fails. A green check that can't go red
  proves nothing.

## 7. Conventions

| Area | Convention |
|---|---|
| `firmware-c5/` | C, ESP-IDF style. App layer in `main/`, lifted components in `components/`. |
| `firmware-cardputer/` | C++. Everything from the OCP client **down** is framework-agnostic plain C++ so a future LVGL move touches only views (DESIGN §7.1). Testable on the host via `tools/hoststub/`. |
| `protocol/` | C99/C++11-clean, data only, no allocation, no includes. |
| `tools/` | Python 3, standard library only. `pyserial` is required *only* for live serial; `--selftest` and `--replay` must keep working without it and without hardware. |

### 7.1 Comments — brief, and only where needed

Code carries *what*. The docs carry *why*. This repo has authoritative
documents for rationale — DESIGN, OCP-SPEC, Rev D, DECISIONS, WORKLOG — so a
comment that explains reasoning at length is duplicating one of them, and
duplicates drift.

- **One line, usually.** Two if it earns it.
- **Cite, don't restate.** `/* Bounded: fault, never hang (Rev D §4.3). */`
  beats a paragraph on why the SX1262 needs it. The reader who wants the
  argument follows the reference.
- **Comment the surprise**, not the obvious. If the code says what it does,
  say nothing.
- **File headers stay short** — what this file is, and which document owns its
  rules. Not a design summary.
- **Never in code:** change history, decision narratives, alternatives
  considered, "we used to do X". That is what WORKLOG and DECISIONS are for.

A file that is majority comment is a signal that its rationale belongs in a
document and the file should point at it instead.

### 7.2 Modules — small, single-purpose, findable

No monoliths. `projectZero`'s `main.c` is explicitly on the do-not-copy list
(DESIGN §11) and that applies to anything we write too.

- **One responsibility per file.** If describing it needs an "and", split it.
- **~300 lines is the soft cap.** Past that, look for the seam. Past 500, there
  is one and you have not found it yet.
- **Follow the module map** in DESIGN §6.1 (probe) and §7.1 (deck). Adding a
  module means updating the map in the same change.
- **Predictable names.** A file should be findable from its responsibility
  without searching the tree — `lora_radio.c` drives the radio, `lora_recon.c`
  runs the survey.
- **The interface is the header.** Keep public surface small and documented in
  a line each; an agent should be able to *use* a module from its header
  without reading the implementation.
- **Isolate what is lifted.** Third-party code lives in `components/`, never
  mixed into our own layers.

This is a maintainability rule and a context-cost rule at once. A session
should be able to pick up work from AGENTS.md, a module map, and one or two
files — not by reading the tree. Every file that has to be read in full to
understand one behaviour is a tax on every future session.

## 8. When to stop and ask

- A task seems to need transmitting (§3.1).
- A `⛔` decision blocks the path (currently D-10).
- Rev D and DESIGN.md disagree about something Rev D doesn't actually cover.
- A change would weaken a stated guarantee — tripwire, bounded wait, bus lock,
  escaping, receive-only.
- You are about to commit something containing real observations.
