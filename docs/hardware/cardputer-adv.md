# Cardputer ADV — board support check (D-12)

Bench, 2026-09-12. Firmware: `firmware-cardputer/bench/adv_check.cpp`
(`pio run -e adv-check`). Libraries pinned: M5Unified 0.2.21, M5GFX 0.2.28,
M5Cardputer 1.1.1, on the `m5stack-stamps3` board definition.

## Results

| Check | Evidence |
|---|---|
| Board detect | `M5.getBoard()` = 24 = `board_M5CardputerADV` (M5GFX autodetect) |
| Display | 240×135; title, text and red/green/blue/white bars correct (Will, visual) |
| Internal I²C (GPIO9 SCL / GPIO8 SDA) | devices at `0x18`, `0x34`, `0x69`; `0x34` is the TCA8418 keyboard controller |
| Keyboard | 52 events captured over USB: letters, digits, space, Shift, Fn, Ctrl, Opt, Alt, Tab, Del, Enter, `` ` `` |
| Grove UART under M5Unified | `ocp_repl.py --gate` through this firmware, 18/18 on three runs |

## Pin facts from the library source

| Function | Pins | Source |
|---|---|---|
| Internal I²C | SCL 9, SDA 8 | `M5Unified.cpp` `_pin_table_i2c_ex_in` |
| **"Port A" external I²C** | **SCL 1, SDA 2 — the Grove UART** | same table |
| microSD (SPI) | CLK 40, MOSI 14, MISO 39, CS 12 — matches Rev D | `_pin_table_sd` |
| RGB LED | 21 | `_pin_table_other0` |
| Keyboard interrupt | 11 | M5Cardputer `TCA8418.cpp` |
| Audio (mic/speaker I²S) | BCK 41, WS 43, MIC in 46, SPK out 42 | `M5Unified.cpp` ADV cases |

None collide with Rev D's rear-header allocation.

## Rules this imposes on the deck

- `Ex_I2C` is only *started* by `external_rtc`/`external_imu` (default off) or
  by an included M5 display-unit header. Leave both alone, or I²C lands on the
  Grove link.
- `M5.Ex_I2C.isEnabled()` reports 1 here: it means the port was assigned
  (`setPort`), not initialised. Not a conflict.
- Start `Serial1` after `M5.begin()`.
- Keyboard: use `KeysState` flags; the translated `word` is inconsistent
  (Ctrl+g → `G`, Fn/Opt/Alt+letter → lowercase).

## Captured sample

```
d12 board=24 adv=1
d12 display=240x135
d12 ex_i2c_enabled=1 (must be 0: GPIO1/2 are the Grove UART)   <- see above: assigned, not started
d12 in_i2c: 0x18 0x34 0x69
d12 key word="A" fn=0 shift=1 ctrl=0 opt=0 alt=0 ...
d12 key word="G" fn=0 shift=0 ctrl=1 opt=0 alt=0 ...
d12 key word="`" fn=0 shift=0 ctrl=0 opt=0 alt=0 ...
```

The `must be 0` label in the firmware was my wrong expectation; the gate result
is the real coexistence test.
