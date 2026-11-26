# bench-cc1101-driver — Oscilla's CC1101 layers on a XIAO ESP32-S3

**Bench instrumentation, not Oscilla firmware.** It compiles the probe's
`firmware-c5/main/cc1101_*.c` unchanged (`src/driver_unity.c`) behind an
Arduino SPI transport. That lets the driver be A/B-tested against the
SmartRC reference sketch (`bench-cc1101-s3`, archived at tag `archive/cc1101-bench`) on the
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
all SPI), `cycle N` (N full reset/apply/rx_start bring-ups), `rxcyc N` (N rx
stop/start pairs).

**Automated power cycling.** Wire the CC1101 VCC to **D4 (GPIO5)** instead of
3V3 and send `vccgpio`. `pc N OFF_MS` then runs N cold starts with power-on
reset only, and `pcr N OFF_MS` adds SRES. While off, every CC1101 line is an
input with pull-down, so nothing back-powers the chip. The `P,` line reports
how many starts read the POR FREQ2 (`por=`) and how many kept their
configuration through the gap (`retained=`). D4 is not driven until
`vccgpio`, so after an S3 reset the module stays unpowered until that
command is sent.

`bench-cc1101-s3/bench_run.py` (Flipper bursts) and an `rtl_433`
cross-reference both work against this output unchanged.

## On the C5 (shared bus with the SX1262)

Build and flash `./tools/build_firmware.sh --cc1101-bench --probe-only` and
`./tools/flash_c5.sh --cc1101-bench`. The probe then serves this same console
on its USB port, with OCP still on Grove. It adds `lload on|off`, which runs
SX1262 receive-only RSSI sweeps as bus load. `coexist.py` compares the
CC1101 floor, unexplained raised windows and Flipper detection with that
load off and on (`--stress` adds bring-up/RX cycling under load). The C5 has
no GDO0 pin, so the edge column is always 0.

### Bitstream capture and decoding (C5)

`mode fifo 25000` samples OOK into the CC1101 RX FIFO, and `cc1101_capture`
drains it every 8 ms (`drain N` sets 1–40 ms). Raw samples print as
`B,t_ms,n,hex`, which `raw off` hides. Decoded packets print as `O,` lines:

    O,t_ms,princeton,24,a5c3e1,frames=1,te=399,code=0xa5c3e1
    O,t_ms,lacrosse_tx141thbv2,40,c082513811,frames=4,te=240,id=192,ch=0,batt_low=1,test=0,temp_c10=93,hum=56

The `I` line adds `fifo_overflows` (the chip's FIFO overflowed),
`fifo_dropped` (the console fell behind), `fifo_late`, `ook_trains` and
`ook_packets`. `fifo_runs.py` checks Princeton run lengths in the raw
samples. `decode_xref.py` checks decoded packets against `rtl_433` and a
Flipper (`--transmit`).

`ocp_xref.py` runs the same comparison through the production path instead:
a normal `--bench` probe build, the OOK lane over OCP (`ook_listen`, merged
`kind=ook` events, OCP-SPEC §15), optionally with the Wi-Fi sniffer and
LoRa RX running too (`--concurrent`). It also needs `--transmit`.

### All-lanes soak

`soak.py` runs the Wi-Fi sniffer, LoRa RX and OOK RX together on a `--bench`
probe for `--hours`, rotating the PHY lane through a scan and a BLE scan every
`--cycle-min` minutes. It records counters only (heap, lane states, event
counts, latency) to `output/soak/<timestamp>/`. `--rtl` counts LaCrosse
reports through `rtl_433`; `--flipper <port> --transmit` sends the made-up
Princeton key A5C3E1 every `--burst-min` minutes. `--summarize <dir>`
re-reads a finished run.
