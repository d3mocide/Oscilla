# P6 — Current budget: measurement method

> **Status: method only. Nothing here has been measured.**
> Every number in Rev D §9's budget table is still provisional, and this
> document does not change that. It exists so the bench session, when it
> happens, produces numbers that can actually set an interlock threshold —
> rather than a single figure with no recorded load behind it.
>
> Results get appended to this file under *Results*, which is empty on purpose.

## 1. What is waiting on this

| Gated thing | Why it needs a measured number |
|---|---|
| `radio_arbiter.c`'s power interlock | Ships as a stub. `OCP_ERR_BUDGET` is defined in [`ocp.h`](../../protocol/ocp.h) and spec'd in [OCP-SPEC §5.3](../../protocol/OCP-SPEC.md), but **no firmware path emits it** — there is no measured headroom to refuse against. |
| [D-4](../DECISIONS.md) — Grove-powered operation | Rev D §9: the C5+Wio provisional 910 mA sits close to the XIAO's nominal 1 A 3.3 V path. Grove power is not evaluated until that gap is measured, not estimated. |
| Any mention of battery operation | Rev D §9: *"Measure at the module pins under simultaneous activity before approving battery operation."* |
| [P6 exit gate](../../ROADMAP.md) | A documented budget in this directory, an interlock that reflects it, and a clean multi-hour soak. |

## 2. Scope — what is measurable now, and what is not

P6's entry gate is P2–P5 exits met. **It is not met:** P4 (GNSS) is mid-flight
and P5 (external TFT) has not started, so the GNSS and TFT loads do not exist
on the bench and the *combined* soak cannot be run.

The **probe-side RF subset can be measured now** — P2 and P3 are both closed,
so Wi-Fi and LoRa RX both run on real hardware today. That subset is also the
part the interlock actually arbitrates: the arbiter only ever decides between
the PHY lane and the LoRa lane. Measuring it early banks most of the interlock
work without waiting on the deck's peripherals, the same way P7's wardrive
writers were started early.

Deck-side loads (TFT backlight, SD, GNSS) are a separate measurement on a
separate board and are explicitly **out of scope** for the early pass. Rev D
§9 notes the ordered TFT's full-backlight current is unmeasured and that no
value from the previous display example carries over.

## 3. Where to measure — the part that is easy to get wrong

Rev D §9 says **measure at the module pins**, and its budget table is
denominated in **mA at 3.3 V**. Two consequences:

- **An inline USB power meter does not produce a budget-table number.** It
  measures ~5 V *upstream of the XIAO's regulator*, so it includes regulator
  loss and the native USB-serial peripheral. Rev D §9 is explicit: *"Do not add
  currents measured at different voltages directly"*, and converting requires
  `Vout × Iout / (Vin × efficiency)` — with no measured efficiency figure for
  the delivered board. A USB meter gives useful **relative deltas between radio
  states** and a system-level sanity check. It does not give the 3.3 V rail
  number the interlock needs.
- **Feeding 3.3 V directly at the XIAO's 3V3 pad** bypasses the onboard
  regulator and measures the C5 + Wio load at exactly the voltage the budget
  table uses. This is the measurement point to aim for. **Verify against the
  delivered XIAO revision's schematic first** — Rev D §9 already requires
  reviewed USB/Grove isolation for the delivered revision, and back-feeding a
  regulator output is revision-specific. Never have USB connected while doing
  this.

## 4. Instrument — what to get

Nothing on the bench measures current today. Requirements, taken from Rev D §9
rather than invented: measure **at the module pins**, at **3.3 V**, under
**simultaneous activity**, and capture **actual peaks** — not just averages.
That last requirement is what rules most cheap options out: Wi-Fi RX bursts and
the SX1262's RX ramp are short, and a 1–2 Hz USB meter averages straight
through them.

**Recommended: Nordic Power Profiler Kit II (PPK2)**, roughly $100.

- *Source meter* mode supplies 0.8–5.0 V **and** measures the current it is
  supplying — so it powers the probe at 3.3 V and measures at that same point.
  That is Rev D's stated measurement point, directly, with no efficiency
  conversion.
- ~100 ksps, sub-µA floor to ~1 A, which covers idle through peak RX.
- Logs CSV on the host, so its trace can be aligned against the probe's own
  `uptime_ms` (see §5).
- **Caveat worth knowing before buying:** its source-meter ceiling is ~1 A,
  which is the *same* order as Rev D's 910 mA provisional figure for C5+Wio.
  If the real combined draw is near that, readings may clip at the top of the
  range. Its *ampere meter* mode (external supply, PPK2 measures in series)
  avoids the ceiling and is the fallback if clipping shows up.

**If budget allows: Joulescope JS110/JS220**, roughly $400–900. Wider dynamic
range and a cleaner story at both extremes; strictly better instrument, harder
to justify unless power work continues past P6.

**Do not rely on:** an inline USB meter (UM25C and similar, ~$25) as the
primary instrument — wrong measurement point, wrong voltage, far too slow for
peaks (§3). Worth owning anyway as a sanity check and for relative deltas, just
not for setting a threshold.

**A bench PSU with a current readout** is a reasonable middle option if one is
already available — right measurement point if it feeds 3.3 V directly, but
typical readouts update too slowly to answer Rev D's "measure actual peaks".

## 5. Bench setup — and the USB conflict to design around

There is a practical conflict: **the C5's USB is both its power path and its
serial console.** If the PPK2 sources 3.3 V while USB is plugged in, two
supplies fight, and the measurement includes whatever USB is delivering.

The project already has the pieces to avoid this, from P1:

```
host ──USB── Cardputer (grove-bridge firmware) ──Grove UART── probe
                                                               │
                                              PPK2 ── 3V3 ─────┘  (probe USB unplugged)
```

- Probe runs the **UART build** (`build-uart`, `[STATUS]` reports `link=uart0`)
  and is powered **only** from the PPK2 at the 3V3 pad.
- Control reaches it over Grove through the deck's `grove-bridge` firmware —
  the same path [`link-bringup.md`](link-bringup.md) used for the P1 gate.
- **Probe USB physically unplugged.** Not idle — unplugged.
- Grove red (5 V) stays disconnected and insulated, per Rev D §9 and AGENTS.md
  gotcha 15. The probe's supply is the PPK2 and nothing else.
- Rev D §9: both endpoints powered before signal lines are connected, and
  signals disconnected before powering down only one side. The Cardputer's
  Grove pull-ups can inject current into an unpowered C5.

## 6. State matrix to measure

Each state is a dwell long enough to capture steady state plus several RX
bursts. Record **idle / mean / peak** for each — a single number per row cannot
set a threshold.

| # | State | How to reach it | What it tells you |
|---|---|---|---|
| 1 | Probe idle, radios down | `stop` | Floor. Everything else is a delta on this. |
| 2 | Wi-Fi scan | `scan_networks` | Managed-scan draw, includes band switching |
| 3 | Wi-Fi promiscuous, 2.4 GHz | `set_band 24`, `start_sniffer` | Continuous RX, hopping |
| 4 | Wi-Fi promiscuous, 5 GHz | `set_band 5`, `start_sniffer` | DESIGN §6.2 uses a 5 GHz sweep as its worst case; no source states it actually costs more than 2.4 — measure, don't assume |
| 5 | LoRa RX only | `lora_config …`, `lora_listen` | SX1262 lane alone, incl. TCXO |
| 6 | **Both lanes concurrent** | 5, then `start_sniffer` | **The number the interlock exists for** |
| 7 | Back to LoRa only | `stop phy` | Confirms the PHY delta in isolation |
| 8 | Back to Wi-Fi only | from 6, `stop lora` | Confirms the LoRa delta in isolation |

Rows 7–8 exist because the scoped `stop` from [D-16](../DECISIONS.md) makes
them possible: one lane can be dropped without disturbing the other, so each
lane's contribution can be isolated *within a single continuous trace* instead
of reconstructed across separate runs with separate warm-ups.

Row 6 is the one that matters. Rev D §10 notes the SX1262 "can operate
alongside them subject to power and interference testing" — row 6 **is** that
testing.

## 7. Aligning the trace to the states

The probe cannot measure current (no current-sense hardware on the C5 — any
figure firmware reported would be fabricated), but it can timestamp its own
state transitions, which is the half that makes a meter trace interpretable.

`[STATUS]` already carries `owner=`, `lora=` and a monotonic `uptime_ms`. The
intended method: a host script walks §6's sequence, dwelling on each state and
recording `uptime_ms` at every transition, then that log is aligned against the
meter's CSV to slice the trace by state.

A `tools/ocp_repl.py --power-sequence` mode was sketched for this and
**deliberately not written yet** — it should be built against the instrument
actually purchased, since the alignment details depend on the meter's own log
format and clock.

## 8. What the numbers have to produce

The pass condition is not a figure, it is three things the figure supports:

1. **Interlock thresholds.** Measured headroom on the 3.3 V path against row 6,
   turning `OCP_ERR_BUDGET` from a reserved code into a reachable refusal — or
   demonstrating the headroom is sufficient and the interlock can stay
   permissive, which is an equally valid documented outcome.
2. **A D-4 answer.** Whether Grove-powered operation is viable, with the
   measured margin stated.
3. **A clean multi-hour soak** at the worst-case state, logging resets,
   timeouts, serial overruns and supply dips per the P6 work items.

Per this directory's README: **record the instrument and the conditions, not
just the number.** A current figure without its load cannot set a threshold.

## Results

*Empty — no measurements taken. See the status note at the top.*
