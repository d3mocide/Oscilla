# bench-power-meter: P6 current meter

**This is not Oscilla firmware.** It's bench instrumentation for a spare XIAO
ESP32-S3, built for the P6 measurement in
[`docs/hardware/power-budget.md`](../docs/hardware/power-budget.md). It does
two jobs:

- it reads an INA226 wired in series with the probe's supply
- it bridges the probe's OCP UART to the laptop, taking over the Cardputer's
  `grove-bridge` role

Both streams share one USB port. Current readings and probe state changes
therefore arrive on one clock, in stream order, and `tools/power_sequence.py`
can slice the trace by state without a separate clock to align.

The meter has no radio code. The probe's receive-only guarantee is unaffected:
the meter forwards the host's OCP bytes exactly as it gets them.

## Wiring

```
supply ──► regulator OUT ──► INA226 IN+ ─[20 mΩ]─ IN− (+VBUS) ──► probe rail
Laptop ──USB-C──► XIAO S3                       all GND common
```

| XIAO S3 | Goes to | Notes |
|---|---|---|
| 3V3, GND | INA226 VCC, GND | Powers the INA226 chip only, not the probe |
| D4 / GPIO5 | INA226 SDA | 400 kHz |
| D5 / GPIO6 | INA226 SCL | |
| D1 / GPIO2 (TX) | 470 Ω → C5 **D7** / GPIO12 (RX) | Crossover (AGENTS.md gotcha 12) |
| D0 / GPIO1 (RX) | 470 Ω ← C5 **D6** / GPIO11 (TX) | |

- **Don't use D6/D7 (GPIO43/44) on the S3.** The S3 ROM prints to GPIO43 on
  every reset, and that text would go into the probe's RX even while the
  probe is unpowered.
- **The probe end follows Rev D §3.** The S3 pins above are bench-only. Rev D
  doesn't cover this board, and they're defined in one place:
  [`src/main.cpp`](src/main.cpp).
- **INA226 VBUS must connect to IN−.** Many modules tie them together
  already; if yours doesn't, add a jumper. Leave the address pins at 0x40.
- **The probe runs the UART build, with its USB unplugged** (power-budget.md
  §5).
- **Grove red (5 V) stays disconnected and insulated.**

**Choosing the rail.** The shunt can sit on either feed, and the firmware
doesn't care which. Tell the host script which one with `--rail`:

- **`3v3`** feeds the C5's 3V3 pad from an external 3.3 V regulator. This
  measures the budget-table number (Rev D §9).
- **`5v`** feeds the C5's 5V pin, so the XIAO's own regulator is in the path.
  These figures include that regulator's losses. They are useful for
  comparing states, and for the Grove-power question (D-4).

**Power order.** The meter only enables its UART after the rail has read
≥ 3.0 V for 200 ms. It drops the UART, leaving both pins as plain inputs,
the moment the rail falls below 2.7 V. So plugging and unplugging the supply
never drives the UART into an unpowered C5 (Rev D §9).

## Shunt and INA226 setup

- **Shunt:** 20 mΩ, 1%, low-TCR, 2512. Full scale is 81.92 mV / 20 mΩ =
  4.1 A, with 125 µA per LSB. The module's stock 100 mΩ shunt clips at
  819 mA, below Rev D's 910 mA planning figure.
- **INA226 config `0x4007`:** no averaging, 140 µs conversions for shunt and
  bus, continuous mode. Each current sample integrates over 140 µs, so a
  spike shorter than that is averaged in rather than caught as a peak. The
  shunt is only measured while the bus conversion isn't running. This is
  much coarser than a PPK2 (§4).
- **Calibrating:** solder joints add milliohms, which is several percent at
  20 mΩ. Pass a known current through the shunt and compare against a DMM,
  then use the corrected value with `--shunt-mohm`.

## Build and flash

```sh
source ../tools/env.sh
pio run -t upload --upload-port /dev/serial/by-id/<the XIAO S3>
```

Always use the `by-id` path: the Espressif boards swap `ttyACM` numbers
(gotcha 9).

## The USB stream

The meter adds nothing to the stream until `#stream on`, so with streaming
off it's a plain probe bridge and `ocp_repl.py` works through it unchanged.

**Host → meter.** A line that starts with `#` is a meter command and is never
forwarded to the probe. Every other byte passes straight through.

| Command | Effect |
|---|---|
| `#info` | Replies `#PWR info …`: INA226 detected, probe power state, settings |
| `#stream on` / `#stream off` | Start or stop `win` and `probe` lines |
| `#window <ms>` | Window length, 10–10000 ms (default 50) |
| `#shunt <µΩ>` | Shunt value used for scaling (default 20000) |
| `#sag <mV>` | Latching bus-undervoltage alert threshold; 0 turns it off (default 3150) |
| `#mark <label>` | Replies `#PWR mark t_us=… label=…`, timestamped when the meter receives it |

Settings replies are `#PWR ok cmd="…"` or `#PWR err msg="…"`.

**Meter → host.** Lines start with `#PWR`, and the meter only inserts them
between complete probe lines. To an OCP parser they're noise
(OCP-SPEC §4.1). If a probe line stays incomplete for over 1 s, the meter
ends it with a newline so its own output can't be held up forever, and
counts that in `cuts`.

```
#PWR win t0_us= t1_us= n= i_ua= i_min_ua= i_max_ua= vbus_mv= vbus_min_mv= sag= clip= cuts= txdrop= i2c_err= lost=
#PWR probe t_us= state=up|down vbus_mv=
```

| Field | Meaning |
|---|---|
| `t0_us`, `t1_us` | Window start and end on the meter's clock. A window runs longer than configured if its line can't go out yet (no samples are lost). |
| `n` | Number of current samples in the window |
| `i_ua` | Mean current over the window |
| `i_min_ua`, `i_max_ua` | Lowest and highest single sample |
| `vbus_min_mv` | Lowest rail voltage, sampled every 10 ms |
| `sag` | 1 if the INA226's undervoltage alert tripped at any point in the window, including dips shorter than 10 ms |
| `clip` | Samples at the ADC's full scale |
| `cuts`, `txdrop`, `i2c_err`, `lost` | Running totals since boot. A rise within a state marks that state invalid. |
