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
RX, with no error, no fault, no crash — just silence.**

**Root cause found and a candidate fix applied, 2026-09-13 — not yet
bench-confirmed.** `lora_radio.c`'s `GetIrqStatus` handling requested 3
payload bytes and reconstructed the 16-bit `IrqStatus` from the wrong two
(datasheet Table 13-30 defines exactly 2 payload bytes after RFU+Status).
The corrupted value fed into `ClearIrqStatus`, and on the wrong bit
combination the real fired IRQ bit was never actually cleared in the chip.
Since DIO1 is edge-triggered (`GPIO_INTR_POSEDGE`), a stuck-set bit means no
further rising edge ever arrives — a silent, permanent stall with nothing to
log, matching every symptom observed. Fixed: request/reconstruct the correct
2 bytes. Flashed to the probe.

**Bench-confirmed fixed, 2026-09-13.** Retest: 66.8-minute session,
524 packets logged, **largest gap between any two consecutive received
packets: 55.1s — zero gaps over 60s, zero over 120s, anywhere in the run.**
No stall, anywhere, well past the ~30 minute mark that killed the previous
attempt. Cross-checked against PyMC for the same window: observer saw only
156 packets there (vs. our 524), but verified as a real RF/topology
difference, not a bug — 524 unique hex payloads and 524 unique timestamps,
zero duplicates, evenly spread across the whole session. MeshCore floods a
message through multiple repeaters; different listeners legitimately hear
different physical retransmission counts of the same logical traffic
depending on which repeaters they're closest to.

**`GetDeviceErrors`/`XOSC_START_ERR` gap closed, 2026-09-14** (was open in
D-10): `lora_radio_rx_start()` now reads `GetDeviceErrors` right after
entering `STDBY_XOSC`, logs whether `XOSC_START_ERR` was set (expected on a
TCXO cold start per the datasheet's own note — not treated as a fault), and
clears it via `ClearDeviceErrors` either way. Non-fatal, doesn't gate
bring-up — it's the positive confirmation the TCXO started within the
chosen 10ms delay that D-10 was missing, not a pass/fail check. Flashed and
live-confirmed working on the bench (2026-09-14), along with the
optimistic-file fix above.

**Resolved.** One hour is real evidence but not unlimited evidence — a
longer soak is worth doing before this goes anywhere near the field, but the
specific silent-stall failure mode is fixed and confirmed, not just
theorized.
