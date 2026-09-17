# Hardware bench notes

Measurements, photographs and bring-up records from real hardware. The
*design* authority is [`Research/c5-backpack-design.md`](../../Research/c5-backpack-design.md)
(Rev D) — this directory is what the bench actually showed, which is a
different thing and is allowed to contradict the plan.

Expected contents as phases land:

| File | Phase | Contents |
|---|---|---|
| `link-bringup.md` | P1 | Grove UART bring-up: boot-noise capture, resync behaviour, reset recovery |
| `lora-harness.md` | P3 | Wio-SX1262 harness continuity check, BUSY/TCXO observations, the D-10 resolution and its source |
| `gnss.md` | P4 | ATGM336H baud, NMEA sentences seen, cold/warm fix times |
| `ieee802154-validation.md` | P7 | Controlled passive MAC capture and no-auto-ACK positive-control evidence |
| `shared-spi.md` | P5 | SD + TFT shared-bus soak results |
| `power-budget.md` | P6 | **Measurement method written 2026-09-15; no measurements yet.** Will hold measured current at the module pins under combined load, superseding the provisional Rev D §9 table and setting the arbiter's interlock thresholds |

Record the instrument and the conditions, not just the number. A current
figure without the load it was measured under cannot set an interlock
threshold.
