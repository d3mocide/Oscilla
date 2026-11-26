# External display power gate — proposed harness addendum

**Revision:** E-DISPLAY-GATE addendum (proposed)
**Date:** 2026-11-22
**Status:** Proposed; not built, not bench-validated. Rev D
([`Research/c5-backpack-design.md`](../../Research/c5-backpack-design.md))
remains the pin authority until this is approved and qualified.
**Scope:** The GMT028-05 V1.1 external ST7789 panel (Rev D §5.4) only. No
change to the SD card, the internal display, GNSS, or the Grove link.

## 1. Problem

Idle display sleep (WP-5) sends the ST7789 `DISPOFF`/`SLPIN` sequence. The
GMT028-05 exposes no backlight pin (Rev D §5.4): its LEDs run whenever VCC is
powered, so the controller sleeps but the backlight keeps drawing current and
the undriven panel shows white (operator observation, 2026-11-22). Firmware
cannot reduce the dominant load. Displaying black instead would look off but
draw slightly more.

## 2. Design

One deck GPIO gates the panel's supply **and** isolates its signal lines, all
inside the display harness.

| Element | Connection | Purpose |
|---|---|---|
| `DISP_EN` | Cardputer EXT3 / **S3 GPIO4** (Rev D §7: unused, not a strap) | High = panel on. 100 kΩ pull-down, so the panel is off and isolated at reset. |
| High-side load switch (e.g. TPS22918, or P-MOSFET such as AO3401 with gate pull-up and NPN/N-FET driver) | In series with the panel VCC branch only, after the separate 3.3 V regulator | Removes panel logic and backlight power. GNSS stays on the unswitched branch. Prefer a part with output discharge so the panel rail falls cleanly. |
| 74LVC126 (quad, **active-high** OE), or 74LVC1G126 per line | Powered from the **unswitched** 3.3 V; inputs from the deck's SCK (GPIO40), MOSI (GPIO14), DC (GPIO6) and CS (GPIO5); outputs to the panel. OE tied to `DISP_EN`. | With the panel off, its pins see only Hi-Z outputs, so its protection diodes cannot clamp the shared SD SCK/MOSI lines or be back-powered. The SD side sees only buffer inputs. |
| RST (GPIO3) | Through a 5th buffered line (second '126 or a '1G126) | Same back-power reason. GPIO3 is an S3 JTAG strap (Rev D §7); the buffer input does not load it. |

Without the buffer, a VCC-only switch would leave SCK/MOSI connected to an
unpowered panel, which is the SD disruption this design exists to avoid.

Signal delay through LVC buffers is a few ns, negligible at the 8 MHz bus clock.
Keep the buffer close to the deck end of the harness, with 100 nF decoupling.

## 3. Firmware (after the harness exists)

- Boot: `DISP_EN` stays low until the SD card is initialised (already Rev D
  §5.2's ordering requirement); then drive high, wait for the rail to settle,
  and run the full M5GFX init.
- Idle sleep: stop TFT traffic, release the bus lock, drive `DISP_EN` low.
- Wake: `DISP_EN` high, settle, full re-init under `storage::lock()`, full redraw.
- A build without the gate (current harness) must keep today's `DISPOFF`/`SLPIN`
  behaviour.

## 4. Qualification before adopting

1. Measure panel current awake vs. today's sleep (INA226 power meter on the
   panel VCC branch), then with the gate off: the target is ~0 mA.
2. SD stress (`sd-display-check`-style write/read/verify cycles) with the panel
   **off**, **on**, and toggled during transfers: zero errors.
3. Cold boot and USB/JTAG recovery with the pull-down fitted (GPIO3 strap
   still behaves; GPIO4 is not a strap).
4. Repeated sleep/wake cycles: panel always reinitialises with correct colours.

On success, fold §2 into Rev D §5.4/§7 and record it in the hardware WORKLOG.
