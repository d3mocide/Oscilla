# P1 — Grove link bring-up

Bench, 2026-09-12. Both boards on their own USB; Grove red disconnected and
insulated (Cardputer Grove selector at 5V OUT, so that wire is live — keep it
insulated).

| Board | USB serial | Stable path |
|---|---|---|
| XIAO ESP32-C5 (probe), rev v1.0 / ROM `eco2` | `38:44:BE:1F:4F:A0` | `/dev/serial/by-id/…38:44:BE:1F:4F:A0-if00` |
| Cardputer ADV (deck), ESP32-S3 | `50:78:7D:CE:6D:64` | `/dev/serial/by-id/…50:78:7D:CE:6D:64-if00` |

## Wiring as verified

| Cardputer | → XIAO | Check |
|---|---|---|
| G2 (TX, yellow) | D7 (C5 GPIO12, RX) | GPIO12 low in 32/40 JTAG samples during a zero-byte flood |
| G1 (RX, white) | D6 (C5 GPIO11, TX) | Frames received on the Cardputer |
| GND | GND | — |

Initially wired swapped (TX→TX): both directions silent, and two outputs
contended on one line. Rev D §3's optional 470 Ω series resistors would have
limited that; fit them.

## Result

`ocp_repl.py --gate` through the `grove-bridge` Cardputer firmware, probe on
the UART build: **18/18, four consecutive runs.** `[STATUS]` reports
`link=uart0`.

On the wire during a reset — only ROM text, then the frame:

```
ESP-ROM:esp32c5-eco2-20250121
Build:Jan 21 2025
rst:0xc (SW_CPU),boot:0x18 (SPI_FAST_FLASH_BOOT)
Core0 Saved PC:0x408069ee
SPI mode:DIO, clock div:1
load:0x408556c0,len:0x1714
load:0x4084bba0,len:0xcfc
load:0x4084e5a0,len:0x31ac
entry 0x4084bbaa
[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps= END
```

9 noise lines over Grove vs 59 over USB earlier: app and bootloader logs now go
to the C5's native USB (Rev D §3).

## Pin-level line check (JTAG)

Reads C5 `GPIO_IN_REG` (`0x60091064`) while flooding `0x00` through the
Cardputer. A connected RX line reads low ~80–90% of samples; a dead one never
does. Needs the OpenOCD udev rule (AGENTS.md §5).

```sh
openocd -f board/esp32c5-builtin.cfg -c "adapter serial 38:44:BE:1F:4F:A0" -c init \
  -c 'for {set i 0} {$i < 40} {incr i} { halt; echo [format 0x%08x [read_memory 0x60091064 32 1]]; resume; sleep 40 }' \
  -c shutdown
```

## Open

- **Cause of the post-soldering boot failure is unknown.** After a cold
  power-up the C5 parked in ROM and never ran the app. Flash was rewritten over
  JTAG *before* the RESET that fixed it, so corrupted flash and a
  boot-time condition can't be told apart. If it recurs: halt over JTAG and
  read the ROM text on the Grove line (it prints on UART0) **before** touching
  flash.
- Cold power-up with the Grove link already attached is untested — and Rev D
  §9 says don't power one MCU with signals attached.
