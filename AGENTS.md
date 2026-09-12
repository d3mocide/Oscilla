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
guess*) and **D-12** (Cardputer ADV support in M5Unified, unverified).

## 3. Invariants — do not break these

1. **No transmit verb. Ever.** Oscilla v1 is receive-only on every radio
   ([D-8](docs/DECISIONS.md), [DESIGN §8](DESIGN.md)). The probe's command table
   is the entire capability surface; with no TX verb in it, there is no
   reachable firmware path to transmission. `ocp.h` `#error`s on any
   `OSCILLA_*_TX` flag and `protocol/test_ocp_header.c` fails on any
   transmit-shaped verb name. **If a task appears to require transmitting,
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
6. **The C5 is single-core.** The dispatch task must outrank engine tasks or
   `stop` will not land while an engine is busy (DESIGN §6.4).
7. **The deck's SD and external TFT share one SPI bus.** One lock, SD brought
   up first, TFT write-only, no task bypasses it (Rev D §5.2). This is the
   deck's main source of hard-to-debug failures.
8. **No SPI inside the DIO1 ISR.** Post to the radio task. All hardware waits
   are bounded — fault, never hang (Rev D §4.3).
9. **Bench power rule:** both boards on their own USB, Grove 5 V (red)
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

## 7. Conventions

| Area | Convention |
|---|---|
| `firmware-c5/` | C, ESP-IDF style. App layer in `main/`, lifted components in `components/`. |
| `firmware-cardputer/` | C++. Everything from the OCP client **down** is framework-agnostic plain C++ so a future LVGL move touches only views (DESIGN §7.1). Testable on the host via `tools/hoststub/`. |
| `protocol/` | C99/C++11-clean, data only, no allocation, no includes. |
| `tools/` | Python 3, standard library only. `pyserial` is required *only* for live serial; `--selftest` and `--replay` must keep working without it and without hardware. |
| Comments | Explain *why*, especially the non-obvious constraint. Match the density of the surrounding file. |

## 8. When to stop and ask

- A task seems to need transmitting (§3.1).
- A `⛔` decision blocks the path (D-10, D-12).
- Rev D and DESIGN.md disagree about something Rev D doesn't actually cover.
- A change would weaken a stated guarantee — tripwire, bounded wait, bus lock,
  escaping, receive-only.
- You are about to commit something containing real observations.
