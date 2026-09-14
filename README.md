# Oscilla

Wireless discovery and field telemetry tool for the M5Stack Cardputer ADV.

Oscilla is **two machines with one contract**. A Cardputer ADV (the *deck*)
carries the keyboard, screens, GNSS and storage. A XIAO ESP32-C5 backpack (the
*probe*) carries the radios — Wi-Fi 2.4/5 GHz, BLE, 802.15.4, and a Wio-SX1262
for sub-GHz. They are joined by a 4-pin Grove cable and speak one small,
human-readable line protocol over it.

> **Receive-only, by construction.** No transmit verb is compiled into any
> Oscilla build, so there is no reachable firmware code path to transmission on
> any radio. The probe's command table is the entire attack surface, and it
> contains nothing that transmits. The SX1262 is TX-capable silicon, so the
> guarantee is about *reachable code paths* — stated that way deliberately.
> See [DESIGN.md §8](DESIGN.md) and [D-8](docs/DECISIONS.md).

Status: **P3 complete.** The protocol contract, the Grove link, passive Wi-Fi
scanning, and LoRa RX (Wio-SX1262, real MeshCore traffic decoded end to end)
are all proven on hardware. GNSS (P4) is next. See [ROADMAP.md](ROADMAP.md)
for phase-by-phase detail and exit-gate evidence.

## Why two machines

The deck is a nice ESP32-S3 with a keyboard and a screen; it is not a good
radio platform, and its own Wi-Fi/BLE stay off in v1. The C5 is a good radio
platform with no human interface at all. Splitting them means the UI never
blocks on a radio, a radio fault never takes down the UI, and the probe can be
driven from a laptop with no deck present — which is exactly how P1 is tested.

Geotagging happens on the deck. The probe never sees a position: it streams
timestamped observations, and the deck stamps each one with its own current
fix *and the age of that fix*. That removes GPS, storage and file transfer
from the probe's job entirely.

## Repository map

| Path | What it is |
|---|---|
| [`DESIGN.md`](DESIGN.md) | Master design document — **software authority** |
| [`Research/c5-backpack-design.md`](Research/c5-backpack-design.md) | Rev D — **hardware authority**. Never write a pin constant from anywhere else |
| [`ROADMAP.md`](ROADMAP.md) | Phases P0–P8 with entry/exit gates, and current status |
| [`WORKLOG.md`](WORKLOG.md) | Dated record of what was done, decided, and observed |
| [`docs/DECISIONS.md`](docs/DECISIONS.md) | The D-numbered decision register |
| [`AGENTS.md`](AGENTS.md) | Rules of engagement for coding agents — authority, invariants, gotchas |
| [`SECURITY.md`](SECURITY.md) | Receive-only guarantee, threat model, data handling, disclosure |
| [`protocol/`](protocol/) | **The contract**: `ocp.h` (literals) + `OCP-SPEC.md` (wire behaviour) |
| [`firmware-c5/`](firmware-c5/) | The probe — ESP-IDF |
| [`firmware-cardputer/`](firmware-cardputer/) | The deck — PlatformIO / Arduino |
| [`tools/`](tools/) | Host-side client, parser, and tests. No hardware required |
| [`docs/hardware/`](docs/hardware/) | Bench notes — what real hardware actually showed, allowed to contradict the plan |
| [`docs/brand/`](docs/brand/) | Visual/UI design system for the deck and external panel |

Both firmwares add `protocol/` to their include path and `#include "ocp.h"`, so
a change to the contract breaks both builds until both are fixed. The contract
cannot silently drift.

## Try it without hardware

The protocol and its parser are fully testable on a workstation:

```sh
./tools/check_protocol.sh                 # ocp.h compiles clean; both skeletons
                                          # compile against it; no transmit verb
python3 tools/ocp_repl.py --selftest      # OCP-SPEC.md §9 conformance checklist
python3 tools/ocp_repl.py --replay tools/fixtures/boot_and_scan.txt
```

The replay fixture is a probe session with everything that makes this protocol
awkward baked in: ROM boot chatter, an SSID containing quotes and newlines, an
event interleaved inside a scan frame, an over-length line, an unknown marker
from a future firmware, and a probe that resets mid-frame.

With hardware and `pyserial`, the same tool drives a real probe:

```sh
python3 tools/ocp_repl.py /dev/ttyACM0
```

## Building

Both toolchains are installed but neither is on `PATH` by default — ESP-IDF
needs its own `export.sh`, and PlatformIO lives in the VS Code extension's
private virtualenv. `tools/env.sh` finds both:

```sh
./tools/build_firmware.sh        # builds probe (esp32c5) and deck (esp32s3)
```

or, to work in one of them interactively:

```sh
source tools/env.sh
cd firmware-c5 && idf.py build           # ESP-IDF v5.5.1
cd firmware-cardputer && pio run         # PlatformIO Core 6.2.0
```

> Source `tools/env.sh`, don't add PlatformIO's `penv/bin` to `PATH` yourself:
> it is a virtualenv containing its own `python`, and putting it *ahead* of
> ESP-IDF's interpreter breaks `idf.py` with a misleading
> `No module named 'esp_idf_monitor'`. The script appends it for that reason.

Bench rule while both boards are on the desk: **each board on its own USB, the
Grove 5 V line disconnected and insulated.** Grove-powered operation is not
evaluated until P6 produces a measured current budget.

## A note on what this records

Oscilla's output is a log of where its operator physically was and what was
transmitting around them. A wardrive CSV is a movement history; a KML track is
that history on a map. Those files belong on the deck's microSD, and
`.gitignore` keeps them out of the repository by extension — but the habit
matters more than the rule. Write up conclusions in `docs/hardware/`; never
paste the raw rows. Full policy in [SECURITY.md](SECURITY.md).

## License

MIT — see [LICENSE](LICENSE). Third-party attribution in [NOTICE](NOTICE).
