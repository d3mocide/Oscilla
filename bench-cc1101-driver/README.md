# bench-cc1101-driver — Oscilla's CC1101 layers on a XIAO ESP32-S3

**Bench instrumentation, not Oscilla firmware.** It compiles the probe's
`firmware-c5/main/cc1101_*.c` unchanged (`src/driver_unity.c`) behind an
Arduino SPI transport. That lets the driver be A/B-tested against the
SmartRC reference sketch (`bench-cc1101-s3` on branch `cc1101-bench`) on the
same module before Rev D allocates CC1101 pins (D-15). Receive-only: the
driver cannot issue a transmit strobe or write PATABLE.

## Wiring (bench only, not Rev D)

| CC1101 | XIAO S3 | GPIO |
|---|---|---:|
| SCK | D8 | 7 |
| MISO / SO | D9 | 8 |
| MOSI / SI | D10 | 9 |
| CSn | D3 | 4 |
| GDO0 | D1 | 2 |
| VCC / GND | 3V3 / GND | — |

## Output (115200 baud), same format as `bench-cc1101-s3`

- `I,...`: chip ID, programmed frequency, and the bring-up report (probe,
  reset, `so_edge`, apply counts, `rx`, `calibrated`, drift count).
- `R,t_ms,rssi_max,rssi_mean,gdo0_edges` every 20 ms. RSSI is read at
  `CC1101_RSSI_POLL_MIN_US` spacing.
- `E,drift,...`: the once-a-second profile verify or MARCSTATE check failed.
  The chip is then re-applied.

Commands: `i`, `d` (register dump), `up` (re-apply), `reset` (SRES and
re-apply), `gap N` (µs between RSSI reads), `quiet`/`loud` (stop or resume
all SPI).

`bench-cc1101-s3/bench_run.py` (Flipper bursts) and an `rtl_433`
cross-reference both work against this output unchanged.
