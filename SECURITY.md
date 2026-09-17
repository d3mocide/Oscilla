# Security Policy

> **Current implementation audit:**
> [`docs/SECURITY_CODE_AUDIT_2026-09-16.md`](docs/SECURITY_CODE_AUDIT_2026-09-16.md)
> records open findings and validation gates. It supplements this policy; it
> does not mark the findings resolved.

Oscilla is a **passive wireless-survey instrument**. It listens; it does not
transmit. This document states that boundary precisely, describes how it is
enforced, and explains how to report a problem.

---

## Reporting a vulnerability

Report privately — please do not open a public issue for a security problem.

- **GitHub Security Advisories** — _Security → Report a vulnerability_ on
  [d3mocide/Oscilla](https://github.com/d3mocide/Oscilla) (preferred; keeps the
  discussion attached to the repo)
- **Email** — <info@d3FRAG.net>

Useful in a report: what you did, what happened, which commit, and which target
(probe, deck, or the host tools). A serial capture or a byte stream that
reproduces it is worth more than a description — `tools/ocp_repl.py --replay`
takes a raw file, so a reproducer can be a single text file.

This is a one-person hobby project. There is no bug bounty, and no guaranteed
response time; expect a reply in days, not hours. Anything confirmed gets
written up honestly in [`WORKLOG.md`](WORKLOG.md) along with what it cost.

---

## The receive-only guarantee

> **No transmit verb is compiled into any Oscilla build. With no transmit verb
> in the probe's command table, there is no reachable firmware code path to
> transmission on any radio — Wi-Fi, BLE, 802.15.4, or LoRa.**

Two honest qualifications, stated up front rather than buried:

1. **The SX1262 is transmit-capable silicon.** The guarantee is about
   _reachable code paths in this firmware_, not a claim that the hardware
   cannot physically transmit. Anyone who flashes different firmware to this
   board can transmit; that is a property of the radio, not of Oscilla.
2. **Receiving is not the same as being invisible.** A superheterodyne receiver
   has local-oscillator leakage, and a powered radio is not a silent object.
   Oscilla does not _intend_ to emit, and emits nothing under firmware control.

### How it is enforced

The boundary is structural — a build-time fact, not a runtime toggle:

| Mechanism                                       | Where                               | What it does                                                                                                                                                                                                                                                                                                              |
| ----------------------------------------------- | ----------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| The command table **is** the capability surface | `protocol/ocp.h` → `OCP_VERB_TABLE` | A capability exists iff its verb is registered. All 31 registered verbs are receive-only.                                                                                                                                                                                                                                 |
| Build tripwire                                  | `protocol/ocp.h`                    | `#error`s if `OSCILLA_WIFI_TX`, `OSCILLA_BLE_TX`, `OSCILLA_154_TX`, `OSCILLA_LORA_TX` or `OSCILLA_TX` is defined. A build that tries to enable transmit does not compile.                                                                                                                                                 |
| Verb-name audit                                 | `protocol/test_ocp_header.c`        | Fails the test suite if any registered verb matches a transmit-shaped name.                                                                                                                                                                                                                                               |
| No transmit capability                          | handshake                           | Every advertised capability names a _receive_ capability, so no client — including a future third-party one — can discover or present transmit functionality.                                                                                                                                                             |
| Receive-only source and state audit              | `tools/check_rx_only.py`, `tools/test_check_rx_only.py` | Fails on transmit-capable APIs and mutation-tests the Wi-Fi passive initializer, BLE `passive=1`, 802.15.4 promiscuous mode, and the SX1262 RX opcode allowlist. |
| Passive scanning                                | `firmware-c5/main/wifi_recon.c`, `ble_recon.c` | Wi-Fi scans use an explicit `WIFI_SCAN_TYPE_PASSIVE`; NimBLE discovery requires `params.passive=1`; the audit rejects bypasses or active-state mutations. |
| SX1262 command surface                           | `firmware-c5/main/lora_radio.c`    | `cmd_write`/`cmd_read` accept only the explicit RX/config/read opcode allowlists; SetTx, SetTxParams, and PA configuration are rejected. |
| Event backpressure                              | `firmware-c5/main/ocp_frame.c`     | RF producers submit fixed-size typed records to `OCP_EVENT_QUEUE_DEPTH`; full queues drop and count by kind, while one task formats and emits them. |
| Transactional replies                           | `firmware-c5/main/ocp_block.c`, `ocp_frame.c` | Block frames are bounded and committed only as a whole after the frame lock is held; lock/write failure drops the complete frame. |
| Protocol-only access                            | architecture                        | The deck cannot reach a radio directly. It speaks only OCP. A compromised or buggy deck cannot inject frames: there is no verb to carry the request and no handler to service it.                                                                                                                                         |

Run it yourself:

```sh
./tools/check_protocol.sh          # includes the tripwire and the verb audit
```

Adding transmit later would mean registering new verbs and removing the
tripwire — a visible, reviewable change, not a configuration flip. That remains
a deliberate future decision ([D-8, D-13](docs/DECISIONS.md)), out of scope for
v1.

---

## Threat model

### Everything received is attacker-controlled

An SSID is whatever an access point chose to broadcast. So are BLE device
names, 802.15.4 payloads, and LoRa packets. None of it is trustworthy input,
and all of it flows through a text protocol on a serial line. The consequences
are designed for:

- **Escaping is mandatory, not cosmetic.** An unescaped newline inside an SSID
  would desynchronise the deck's parser. All text fields are quoted, with
  backslash and quote escaped and every byte outside printable ASCII emitted as
  `\xHH` — reversible byte-exactly ([OCP-SPEC §6](protocol/OCP-SPEC.md)).
- **Decoded bytes are still hostile.** Escaping protects the transport, not the
  consumer. Once a field is decoded it may contain control characters, ANSI
  escape sequences, or bidirectional overrides. Every renderer must re-escape
  before display. `tools/ocp_repl.py` does; the deck's views owe the same duty.
- **Bounded everything.** Both ends cap line length and queue depth and **drop
  rather than block**, so neither a flood of observations nor an over-long line
  can stall an engine or exhaust memory.
- **Unparseable input never advances state.** Any line that is not a known
  marker or an expected row is ignored. This is also what lets the deck ride
  out the probe's ROM boot chatter, which appears on the control line at every
  reset and is unavoidable by design.

### The probe/deck boundary

The probe exposes no radio API — only vetted OCP verbs, all receive-only. This
is the security boundary, and it falls out of the architecture rather than
being bolted on. It also means the probe is independently auditable: its entire
attack surface is one command table you can read in a few minutes.

### What is not defended against

Stated plainly, because a threat model that claims everything is worthless:

- **Physical access.** Anyone holding the hardware can reflash it.
- **A malicious probe.** The deck trusts what the probe reports. A probe
  running modified firmware can lie about what it heard. There is no signing.
- **The serial link itself.** The Grove UART is a short unauthenticated wire
  between two boards in the same enclosure. Nothing is encrypted; it is not a
  channel that crosses a trust boundary.
- **Traffic analysis of the operator.** See data handling below.

---

## Implementation status

The repository contains the current Wi-Fi, BLE, 802.15.4, SX1262 LoRa, GNSS,
OCP transport, and microSD export engines. The receive-only boundary,
hostile-field escaping, typed event queue, transactional block framing, strict
command/GNSS validation, and failed-init capability gating are implemented in
source and covered by the host security suite. The precise finding-by-finding
record is [`docs/SECURITY_CODE_AUDIT_2026-09-16.md`](docs/SECURITY_CODE_AUDIT_2026-09-16.md).

Build and host evidence is not hardware evidence. The 2026-09-17 bench run
verified current-image 802.15.4 capture/no-ACK behavior, Wio lane isolation,
promiscuous sniffing, and three paced UART flood/stop runs. The initial SD/power
follow-up found that removing and reinserting a live card left the logger
reporting `wardrive_open=1` while the SD driver reported repeated card-status
failures. The working-tree fix now marks storage absent and closes the logger
on verified failure; a later explicit logging request performs one remount
attempt. The final hardware run changed the Home-screen indicator from
`SD: READY` to `SD: ABSENT`, then remounted and opened the next session without
rebooting. The recovered files have not yet been inspected byte-for-byte. The
remaining boundary includes an independent 802.15.4 positive-control repeat,
fully saturated USB-to-Grove flood behavior, complete Wi-Fi/LoRa receive-only
coverage under suitable live sources, host-side file-integrity inspection, and
clean CI builds resolving the pinned inputs. Those are named as partial or
unverified rather than implied by a green compile.

---

## Data handling — read this part

**Oscilla's output is a record of where its operator physically was and what
was transmitting around them.**

A wardrive CSV is a movement history. A KML track is that history on a map. An
NMEA capture is raw position fixes. A coredump can contain buffered
observations. Publishing one of these discloses your home, your routine, and
the networks of everyone you passed.

- Recorded data belongs on the deck's microSD, not in this repository.
- [`.gitignore`](.gitignore) excludes `*.csv`, `*.kml`, `*.gpx`, `*.nmea`,
  `*.pcap`, `*.log`, coredumps and session directories — but treat that as a
  safety net, not permission to be careless.
- Write **conclusions** in `docs/hardware/`. Never paste raw rows.
- Before sharing a log or a capture for a bug report, strip or blunt the
  positions. A protocol bug almost never needs real coordinates to reproduce —
  and `--replay` takes synthetic input.

---

## Authorized and lawful use

Oscilla is built for surveying spectrum you are permitted to survey: your own
networks, authorized assessments, education, and research.

Passive reception is not uniformly unregulated. Rules on receiving, recording,
and retaining wireless observations — and on the personal data inside them —
vary by jurisdiction, and "I only listened" is not everywhere a defence.
Collecting identifiers tied to people or premises may carry obligations of its
own. **Know the rules where you are before you record.**

Oscilla excludes deauthentication, evil-twin and rogue-AP operation, beacon
spam, jamming, and every other active technique — not as a configuration
default, but by not building them. If you need active assessment capability,
Oscilla is the wrong tool and modifying it to become one is outside what this
project supports.

---

## Scope for reports

**In scope:** parser or framing flaws (desynchronisation, wedging, overflow,
escape handling), anything that reaches a transmit path, capability or
arbitration bypass, memory-safety issues in probe or deck firmware, unsafe
handling of recorded data, and flaws in the host tools under `tools/`.

**Out of scope:** the fact that the SX1262 hardware can transmit under
different firmware; physical attacks; issues in ESP-IDF, PlatformIO, M5Unified
or other upstream dependencies (report those upstream — tell us too if Oscilla
is affected); and vulnerabilities in the networks Oscilla observes, which are
not ours to disclose.
