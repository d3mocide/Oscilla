<div align="center">

<a href="docs/brand/oscilla-master-brand-ui-guide.html">
  <img src="docs/brand/assets/lens-core.svg" alt="Oscilla Lens Core Logo" width="128" height="128" />
</a>

# OSCILLA

### _Observe the noise._

[![Zero Transmit](https://img.shields.io/badge/INVARIANT-ZERO_TRANSMIT-85E37D?style=flat-square&labelColor=060806)](AGENTS.md#3-invariants--do-not-break-these)
[![Architecture](https://img.shields.io/badge/CONTRACT-TWO_MACHINES_%C2%B7_ONE_WIRE-F5D658?style=flat-square&labelColor=060806)](protocol/OCP-SPEC.md)
[![Status](https://img.shields.io/badge/ROADMAP-P3_COMPLETE_%C2%B7_P4_NEXT-87D5CB?style=flat-square&labelColor=060806)](ROADMAP.md)
[![Dual Radios](https://img.shields.io/badge/RADIOS-WI--FI_%C2%B7_BLE_%C2%B7_802.15.4_%C2%B7_SX1262_%2B_CC1101-6C8A67?style=flat-square&labelColor=060806)](docs/hardware/c5-dual-radio-wiring.md)
[![External Viewports](https://img.shields.io/badge/UI_SYSTEM-320%C3%97240_ILI9341-FAF4D3?style=flat-square&labelColor=060806)](docs/brand/oscilla-master-brand-ui-guide.html#external-display-cards)
[![License](https://img.shields.io/badge/LICENSE-MIT-1D2A1F?style=flat-square&labelColor=060806)](LICENSE)

<br/>

**Wireless discovery, packet dissection, and field telemetry instrument for the M5Stack Cardputer ADV.**  
*An open-source RF instrumentation initiative by **d3FRAG Networks**.*

[Interactive Brand & UI Guide](docs/brand/oscilla-master-brand-ui-guide.html) · [Software Design (DESIGN.md)](DESIGN.md) · [Hardware Authority (Rev D)](Research/c5-backpack-design.md) · [Dual-Radio Wiring (Rev E)](docs/hardware/c5-dual-radio-wiring.md) · [Protocol Spec (OCP-SPEC.md)](protocol/OCP-SPEC.md) · [Decisions](docs/DECISIONS.md)

</div>

---

> [!IMPORTANT]
> **◇ INVARIANT: RECEIVE-ONLY BY CONSTRUCTION**  
> No transmit verb exists in `protocol/ocp.h`, no TX command is compiled into any Oscilla firmware build, and every radio scan is strictly passive (`WIFI_SCAN_TYPE_PASSIVE`). The probe's command table is the entire capability surface — with no TX verb in it, there is no reachable firmware code path to transmission on any radio. Tripwires (`tools/check_rx_only.py`) fail the build if transmit-capable driver APIs are reached.  
> _Stated deliberately._ See [DESIGN.md §8](DESIGN.md), [AGENTS.md §3](AGENTS.md), and [D-8](docs/DECISIONS.md).

---

## 1. Two Machines, One Contract

Oscilla splits physical responsibilities cleanly between two microcontrollers joined by a 4-pin Grove cable (UART 115200 8N1 crossover). They speak the **Oscilla Control Protocol (OCP)**, compiling against the shared `protocol/ocp.h` header so the contract cannot silently drift.

```text
┌──────────────────────────────────────────────┐       4-Pin Grove Crossover Cable       ┌──────────────────────────────────────────────┐
│                  THE DECK                    │      (UART 115200 8N1 · Crossover)      │                  THE PROBE                   │
│            M5Stack Cardputer ADV             │◄───────────────────────────────────────►│        Seeed XIAO C5 Dual-Radio Backpack     │
│                                              │      G2 (TX) ──► D7   G1 (RX) ◄── D6    │                                              │
│  • ESP32-S3 Dual-Core (UI, Storage, GNSS)    │                                         │  • ESP32-C5 RISC-V (2.4 / 5 GHz Wi-Fi 6)     │
│  • 56-Key Matrix Keyboard + Navigation D-Pad │           OSCILLA CONTROL PROTOCOL      │  • Bluetooth 5.0 LE (Coded PHY / 1M / 2M)    │
│  • ST7789 240×135 Built-in Screen (Primary)  │           `protocol/ocp.h` Contract     │  • IEEE 802.15.4 (Zigbee / Thread)           │
│  • ILI9341 320×240 External Panel (SPI DMA)  │                                         │  • Wio-SX1262 LoRa (862–930 MHz / Meshtastic)│
│  • MicroSD FAT32 Geotagged Storage Logs      │    Framed ASCII Verbs · Escaped Octets  │  • TI CC1101 (387–464 MHz / Legacy OOK/FSK)  │
│  • MAX-M10S GNSS Fix & Timestamping Engine   │    `[HELLO]` `[LORA]` `[WIFI]` `[BLE]`  │  • Shared Hardware SPI Bus (D8/D9/D10)       │
└──────────────────────────────────────────────┘                                         └──────────────────────────────────────────────┘
```

### Dual Sub-GHz Architecture (SX1262 + CC1101)

The C5 probe integrates **two complementary sub-GHz receive peripherals** over a shared hardware SPI bus ([`docs/hardware/c5-dual-radio-wiring.md`](docs/hardware/c5-dual-radio-wiring.md), [D-15](docs/DECISIONS.md)):

| Peripheral           | Primary Band              | Demodulation Target              | Example Target Traffic                                       |
| -------------------- | ------------------------- | -------------------------------- | ------------------------------------------------------------ |
| **Seeed Wio-SX1262** | **915 MHz** (862–930 MHz) | LoRa Chirp Spread Spectrum       | Meshtastic mesh, LoRaWAN sensors, decentralized telemetry    |
| **TI CC1101**        | **433 MHz** (387–464 MHz) | Narrowband OOK, ASK, 2-FSK, GFSK | Legacy ISM weather stations, TPMS, security sensors, remotes |

- **Pin-Efficient Shared Bus:** Both modules share SPI clock (`D8` / GPIO8), MOSI (`D10` / GPIO10), and MISO (`D9` / GPIO9). Wio chip select is dedicated on `D4` (GPIO23) and CC1101 chip select on `D3` (GPIO7). No extra pins are needed for CC1101 (SPI strobe reset + FIFO polling).
- **Mutual Desense Prevention:** Oscilla v1 schedules only one sub-GHz receive engine at a time (`lora_rx` XOR `legacy_rx`) and equips each module with an independent, band-matched antenna.

### Why two machines?

- **Isolation & Robustness:** The deck is an ESP32-S3 with a keyboard and dual screens; its internal radios stay off in v1. The C5 is a dedicated RF platform with zero human interface. The UI never blocks on radio tasks, an RF fault never freezes the display, and the probe can be operated standalone over USB from a laptop workstation.
- **Authoritative Geotagging:** The probe never handles GPS coordinates. It streams timestamped observation frames; the deck attaches its own MAX-M10S 3D GNSS fix and fix-age before serializing to SD.

---

## 2. Dual Displays & UI System

Oscilla's interface architecture pairs a built-in interactive console with a pure-data external instrument panel:

1. **Primary Screen (ST7789 240×135 @ 16:9):** Integrated Cardputer deck screen. Carries the top system bar (battery, radio status dots, transport status), live navigable lists, interactive menus, and tactical footers.
2. **External Instrument Panel (ILI9341 320×240 @ 4:3):** Secondary SPI DMA landscape panel. Pure data viewport with zero control redundancy, zero buttons, and high-contrast tactical visualizations.

Explore the complete master design system in [`docs/brand/oscilla-master-brand-ui-guide.html`](docs/brand/oscilla-master-brand-ui-guide.html).

### External Viewports (Section 05)

| Viewport          | Instrument Visualization                                             | Purpose & Metrics                                                        | Status        |
| ----------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------ | ------------- |
| **Spectrum**      | Continuous RF waterfall cascade & channel occupancy envelope         | 60% burst threshold, focused-channel RF metrics, interference floor      | P5 shell      |
| **Contacts**      | Polar RSSI proximity reticle & AP-client constellation filaments     | Rogue beacon detection, BSSID/ESSID tracking, PMF/WPA3 audit             | P7            |
| **Drive Log**     | Dynamic N/W geospatial vector moving map & rolling density sparkline | GNSS track, heading rose, GPS accuracy error circle, serialized SD rate  | P4/P7         |
| **Frame List**    | LoRa chirp modulation spectrogram & live Protobuf packet dissector   | Preamble/sync chirp ramps, bitstream pills, hex dump, SNR/RSSI telemetry | **P3 proven** |
| **BLE Beacons**   | Polar advertiser proximity reticle & rotation burst tracker          | Apple Find My / AirTag tracking, RPA epoch rotation bursts, range est.   | P7/P8         |
| **Mesh Topology** | Multi-hop receive-only node graph & route telemetry                  | Meshtastic route tracing, hop boundaries (1H–3H), delivery health        | P8            |

---

## 3. Current Status & Roadmap

Current Milestone: **P3 Complete.**

- Protocol contract (`protocol/ocp.h`) and framing parser proven against hostile conformance fixtures.
- Grove physical crossover link bench-verified with stable bidirectional communications.
- Passive Wi-Fi 2.4/5 GHz scanning verified on hardware.
- LoRa RX verified on hardware: Wio-SX1262 backpack receiving and decoding real MeshCore packets end-to-end with zero RX stalls over extended sessions (>66 min).
- Next Phase: **P4 (GNSS Integration & Deck Geotagging Engine)**.

See [`ROADMAP.md`](ROADMAP.md) for full phase-by-phase entry/exit gates and test evidence, and [`WORKLOG.md`](WORKLOG.md) for session-by-session engineering logs.

---

## 4. Repository Map & Authority

| Document / Directory                                                             | Role & Authority                                                                    | Never                                           |
| -------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------- | ----------------------------------------------- |
| [`Research/c5-backpack-design.md`](Research/c5-backpack-design.md)               | **Hardware Authority (Rev D)**: Pins, rails, bus sharing, electrical limits.        | Write a pin constant from any other file.       |
| [`docs/hardware/c5-dual-radio-wiring.md`](docs/hardware/c5-dual-radio-wiring.md) | **Dual-Radio Authority (Rev E)**: Wio-SX1262 + CC1101 shared SPI pinout & passives. | Share chip selects or combine antenna ports.    |
| [`DESIGN.md`](DESIGN.md)                                                         | **Software Authority**: Architecture, task boundaries, module scope.                | Exceed documented scope without a decision.     |
| [`protocol/`](protocol/)                                                         | **The Contract**: `ocp.h` (literals) + `OCP-SPEC.md` (wire behavior).               | Hardcode a verb or marker string anywhere else. |
| [`docs/DECISIONS.md`](docs/DECISIONS.md)                                         | **Decision Register**: D-numbered records of resolved/deferred questions.           | Resolve a `⛔` decision by guessing.            |
| [`AGENTS.md`](AGENTS.md)                                                         | **Operating Manual**: Invariants, authority hierarchy, and bench gotchas.           | Break receive-only or run `.sh` on Windows.     |
| [`SECURITY.md`](SECURITY.md)                                                     | **Threat Model**: Data handling, wardrive hygiene, disclosure policy.               | Commit field CSVs or sensitive captures.        |
| [`firmware-c5/`](firmware-c5/)                                                   | **The Probe**: ESP-IDF v5.5.1 firmware for XIAO ESP32-C5.                           | Spin in tasks without explicit `vTaskDelay`.    |
| [`firmware-cardputer/`](firmware-cardputer/)                                     | **The Deck**: PlatformIO / Arduino C++ firmware for Cardputer ADV.                  | Bypass the shared SD/TFT SPI bus lock.          |
| [`tools/`](tools/)                                                               | **Host Client & Verification**: CLI parser, REPL, and compliance test suites.       | Require hardware for protocol verification.     |
| [`docs/brand/`](docs/brand/)                                                     | **Visual System**: Master Brand & UI Guide for primary and external displays.       | Deviate from brand color and font tokens.       |

---

## 5. Quickstart & Verification

### Host Verification (No Hardware Required)

You can verify the protocol contract and parser conformance on your workstation using Python 3 and standard build tools:

```sh
# Verify ocp.h compiles clean, invariants hold, and no transmit verbs exist:
./tools/check_protocol.sh

# Run the 21-point OCP-SPEC §9 parser compliance test suite:
python3 tools/ocp_repl.py --selftest

# Replay an adversarial fixture containing boot chatter, escaped octets, and split frames:
python3 tools/ocp_repl.py --replay tools/fixtures/boot_and_scan.txt
```

> **Windows Host Note:** Use native PowerShell / Python commands. Do not execute `.sh` scripts directly in pwsh without bash.

### Interactive Probe REPL (With Hardware)

Connect the XIAO ESP32-C5 probe via USB-C to your workstation (`pyserial` required):

```sh
# Connect to probe via serial port (specify explicit chip type):
python3 tools/ocp_repl.py /dev/serial/by-id/usb-Espressif_...
```

---

## 6. Building Firmware

Both toolchains are installed on the bench machine:

- **ESP-IDF v5.5.1** (`~/esp/esp-idf`) for the ESP32-C5 probe.
- **PlatformIO Core 6.2.0** (`~/.platformio/penv`) for the Cardputer ADV deck.

```sh
# Build both images:
./tools/build_firmware.sh

# Or build individually:
source tools/env.sh
cd firmware-c5 && idf.py build
cd firmware-cardputer && pio run
```

> [!CAUTION]
> **Bench Power Rule:** When testing on the bench, keep both boards connected to their own USB cables, and **leave the Grove 5 V (red wire) disconnected and insulated**. Grove-powered operation is deferred until P6 establishes a measured power budget.

---

## 7. Data Hygiene & Security

Oscilla records the physical whereabouts of its operator and ambient wireless emitters. Wardrive logs, KML tracks, and raw packet captures belong strictly on the deck's local microSD card and **must never be committed to the repository** (`.gitignore` enforces extension rules).

Always sanitize observations and post analytical summaries rather than raw rows. See [SECURITY.md](SECURITY.md) for disclosure policies and data handling rules.

---

## 8. Lineage, Inspirations & Attribution

Oscilla stands on the shoulders of several pioneering open-source embedded RF and wireless reconnaissance projects:

### Lineage & Inspirations

- **[ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) by @justcallmekoko:** Pioneered handheld ESP32 wireless field telemetry, wardriving, and tactical portable UI. Oscilla draws strong conceptual inspiration from Marauder's field utility and wardrive workflows, but structurally diverges by splitting into a dual-machine architecture (Deck + Probe) and enforcing a strict **receive-only invariant** (no deauthentication, no packet injection, zero transmit verbs).
- **[C5Lab projectZero](https://github.com/c5lab) (MIT):** The primary architectural reference for early ESP32-C5 dual-band Wi-Fi (2.4/5 GHz) and IEEE 802.15.4 operations. Oscilla studied projectZero's multi-band sniffer patterns, NimBLE passive scanning, and adaptive channel selection.
- **[esp32-wifi-penetration-tool](https://github.com/risinek/esp32-wifi-penetration-tool) by Martin Risinek (@risinek, MIT):** The upstream foundation from which projectZero descended. Provided foundational ESP32 Wi-Fi frame dissection and promiscuous sniffer hooks.
- **[Meshtastic](https://meshtastic.org/) & MeshCore:** Inspires Oscilla's passive LoRa mesh topology inspection, hop tracing, and Protobuf dissector.

### Code Reuse & Clean-Room Boundaries

To maintain rigorous code health, modularity, and licensing integrity, Oscilla follows strict reuse boundaries ([DESIGN.md §11](DESIGN.md#11-reuse-plan--licensing)):

| Component / Subsystem | Upstream Source | Integration Method | Implementation Status |
|---|---|---|---|
| **802.15.4 Zigbee Recon (`zig_recon/`)** | C5Lab projectZero (MIT) | **To be lifted verbatim** with original headers | Scheduled for P7/P8 (will be isolated under `firmware-c5/components/zig_recon/`) |
| **D-UCB Channel Picker** | projectZero (MIT) | **Clean-room reimplementation** | Discounted-bandit adaptive channel allocation in `firmware-c5/main/` |
| **Wi-Fi Promiscuous Sniffer** | @risinek / projectZero (MIT) | **Clean-room reimplementation** | Passive non-blocking 2.4/5 GHz frame parser in `firmware-c5/main/` |
| **NimBLE Passive Tracker** | projectZero (MIT) | **Clean-room reimplementation** | BLE beacon, AirTag, and RPA rotation tracking in `firmware-c5/main/` |
| **SX1262 LoRa Driver** | In-house (Oscilla) | **Authored in-house** | Bounded timeouts, Rev D pin mapping, strict RX-only in `lora_radio.c` |
| **Cardputer Deck UI & HAL** | M5Unified / M5GFX (MIT) | **PlatformIO dependency** | Pinned releases (M5Unified 0.2.21, M5GFX 0.2.28, M5Cardputer 1.1.1) |

> **Deliberately Excluded Upstream Code:**
> 1. **All transmit & offensive engines:** Oscilla rejects all deauth, beacon spam, PMKID injection, and active attack modules from upstream tools. Oscilla is strictly receive-only by construction ([DESIGN.md §8](DESIGN.md), [D-8](docs/DECISIONS.md)).
> 2. **Monolithic source files:** projectZero's `main.c` monolith is explicitly on the do-not-copy list ([AGENTS.md §7.2](AGENTS.md)). Oscilla enforces single-responsibility modules (~300 lines soft cap).

### License & Obligations

- **License:** [MIT](LICENSE) © 2026 d3FRAG Networks.
- **Third-Party Obligations:** Detailed in [`NOTICE`](NOTICE). All lifted files retain original author headers; third-party components stay quarantined in `firmware-c5/components/` and are never mixed into Oscilla's application layer.
