# bench-cc1101-s3 — CC1101 RSSI reference for D-15

**This is not Oscilla.** It is a throwaway receive-only Arduino sketch for a
spare XIAO ESP32-S3, built to answer one question before D-15 is reopened: does
a CC1101 module show RSSI during known transmissions when run by a known-good
driver (SmartRC-CC1101-Driver-Lib v2.5.7, pulled as a dependency, not
vendored; its licence is a loose grant, so no code from it goes into Oscilla)?
The sketch never calls `SetTx`, `SendData` or `setPA`.

## Wiring (same XIAO labels as the archived C5 harness)

| CC1101 | XIAO S3 | GPIO |
|---|---|---:|
| SCK | D8 | 7 |
| MISO / SO | D9 | 8 |
| MOSI / SI | D10 | 9 |
| CSn | D3 | 4 (10 kΩ pull-up to 3V3) |
| GDO0 | D1 | 2 |
| VCC / GND | 3V3 / GND | — |

Match by the module's signal names, not header position.

## Output (115200 baud)

- `I,partnum=..,version=..,freq_reg=..,mhz_at_26=..,mhz_at_27=..` — chip ID and
  the programmed frequency under both common crystals.
- `D,00=..,...` — register dump 0x00–0x2E (send `d`; `i` reprints `I`).
- `R,t_ms,rssi_max,rssi_mean,gdo0_edges` — every 20 ms: peak/mean RSSI of
  back-to-back samples and demodulated-OOK edges on GDO0 (async mode).
- `m433.92` retunes.

## Procedure

```sh
pio run -e xiao-s3 -t upload --upload-port /dev/serial/by-id/<xiao-s3>
python3 bench_run.py --s3 <s3> --flipper <flipper> probe          # no TX
python3 bench_run.py --s3 <s3> --flipper <flipper> run --transmit  # after plan sign-off
```

`run` sends made-up Princeton bursts from the Flipper's internal CC1101
(`subghz tx <key> <hz> <te> <repeat> 0`). Results land in `out/` (gitignored).
Pass: a clear RSSI rise and GDO0 edge burst on every Flipper burst.
