# P3 — Wio-SX1262 harness bring-up

Bench, 2026-09-13. Point-to-point harness per Rev D §4 (confirmed mapping —
see AGENTS.md gotcha 16 for how the wrong mapping was nearly used instead).
D-10 (TCXO voltage/delay) resolved from the official SX1261/2 datasheet, not
guessed — see `docs/DECISIONS.md` D-10 for the values and reasoning.

## Wiring as built

Wired without the NSS/RST/RF_SW pull-up/pull-down resistors Rev D §3 calls
for — measured absent on the Wio board with a multimeter (all three read
open, not ~10 kΩ) and wired anyway as a **deliberate bench-only call**: RF_SW
is a Wio *input*, not something that actively drives GPIO25 during the C5's
own boot-strap sampling, so the risk profile is closer to "leave it
unconnected" than "something fights the strap." Still not field-ready by
Rev D's own standard — add the resistors before this goes past bench testing.

## Bring-up result

Full reset -> TCXO -> calibrate -> RX-entry sequence ran fault-free against
the real chip first try: no `hwfault`, no `ESP_ERR_TIMEOUT`, anywhere.
Confirms the SPI (SCK/MISO/MOSI/NSS), NRESET, and BUSY wiring is correct.

Retargeted from an arbitrary bench frequency to MeshCore's real USA/Canada
preset — 910.525 MHz, SF7, BW62.5, CR "5" (SX1262 register value `1`,
their shorthand for 4/5) — confirmed from MeshCore's own `docs/faq.md`, not
guessed. Received real, correctly-decoded packets from a nearby repeater:
RSSI -58 to -74 dBm, SNR 11.5-13.0 dB, physically consistent with a strong
nearby link.

## Cross-reference tool: PyMC observer

Will runs an independent MeshCore observer node with a query API — genuinely
useful for telling "Oscilla's RX is broken" apart from "the mesh is just
quiet," which a soak test alone cannot distinguish (see below).

- **URL:** `https://pymc.int.d3mo.us` (internal network only)
- **Auth:** `X-API-Key` header. Credentials live in `tools/.env`
  (`PYMC_API_URL`, `PYMC_API_KEY`) — gitignored, never commit them, never
  paste the key value into a doc or commit message.
- **Endpoint used for cross-referencing:** `/api/filtered_packets?start_timestamp=<unix>&end_timestamp=<unix>&limit=<n>`
  — returns every packet the observer saw in a real time window, independent
  of anything Oscilla did or didn't log. `/api/stats` and `/api/logs` are
  live-only (a rolling recent buffer, not history) — not useful for checking
  a specific past window.
- Endpoint discovered from the dashboard's own frontend JS bundle
  (`/assets/index-*.js`, grep for `/api/`), not documented anywhere public —
  re-derive the same way if the endpoint list ever needs rechecking.

## Open issue: RX silently stalls after ~30 minutes (unresolved)

**2026-09-13, ~2h soak, cross-referenced against the PyMC observer:**

| Window | Duration | Observer saw | Oscilla logged | Rate (observer) |
|---|---|---:|---:|---|
| 17:19-17:40 (active) | 21.3 min | 201 | 177 | 9.45 pkt/min |
| 17:40-19:25 ("quiet") | 104.8 min | **935** | **0** | 8.92 pkt/min |

The observer's rate barely changed between the two windows (9.45 -> 8.92
pkt/min) — the mesh never went quiet. Oscilla's own receive count dropped
from 177 to 0 at almost exactly the same rate it had been running, and never
recovered for the rest of the soak, while the rest of the firmware (main
loop, heap, UI) stayed healthy the whole time — heap was rock-stable across
the full ~2h15m, confirmed separately.

**Conclusion: something in the RX pipeline silently stops producing `RxDone`
events (or stops being serviced) after roughly half an hour of continuous
RX, with no error, no fault, no crash — just silence.** Not yet root-caused.
Candidates to check, not yet verified either way:

- A known class of real-world SX126x continuous-RX behavior where the
  receiver needs periodic intervention after certain IRQ conditions —
  needs checking against the datasheet/errata, not assumed.
- A bug in `lora_radio.c`'s own IRQ clear/re-arm logic that leaves the chip
  in a state where `RxDone` stops firing after a specific IRQ combination.
- `GetDeviceErrors`/`XOSC_START_ERR` was never polled during this run (noted
  as an open gap in D-10) — a slow clock drift is plausible and unchecked.

**Do not consider LoRa RX field-ready until this is root-caused and fixed.**
The P3 exit gate's literal wording (real packets observed, `stop` releases
cleanly, zero TX verbs) was met before this was discovered — this finding
doesn't unmet it, but it is a real reliability problem sitting on top of an
otherwise-working RX path, and needs to close before this goes beyond bench
testing.
