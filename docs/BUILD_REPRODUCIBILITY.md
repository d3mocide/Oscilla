# Reproducible build inputs

Oscilla's local build gate is intentionally strict about the toolchains that
own hardware behavior:

- ESP-IDF `v5.5.1` for the ESP32-C5 probe;
- PlatformIO Core `6.2.0` for the Cardputer ADV deck;
- PlatformIO platform `espressif32@7.0.1`;
- M5Unified `0.2.21`, M5GFX `0.2.28`, and M5Cardputer `1.1.1` in
  `firmware-cardputer/platformio.ini`.

`tools/build_firmware.sh` asserts the first two versions before either board
build starts. It also refuses a v5.5.1 tree without Espressif's IDFGH-17050
fix (commit `ffc08c2`, released in v5.5.5): on a C5 rev v1.0 with the USB
Serial/JTAG console, the stock v5.5.1 gates UART0's clock at power-on and the
ROM then loops when esptool resets the chip, so automatic flashing fails. The
upstream backport is vendored unchanged as
`tools/idf-patches/v5.5.1-esp32c5-usb-reset-IDFGH-17050.patch`; apply it with
`git -C "$IDF_PATH" apply <patch>`, and CI does the same after cloning. Drop
the patch when the pin moves to v5.5.5 or later. The platform and library versions are declared in
`platformio.ini`; generated `.pio` package state remains a build artifact and
is not treated as reviewable source. CI installs PlatformIO into a dedicated
Python virtualenv so the pinned `pio` executable is on PATH independently of
the runner's user-install layout. CI deliberately adds that virtualenv to
`PATH` only after ESP-IDF has created its own Python environment; otherwise the
ESP-IDF installer rejects the nested virtualenv. The resulting path is written
to the runner environment from a step, rather than using the unavailable
`runner` context at job-level `env`. CI then runs the same host security gate,
the production C5 UART plus deck build, and a probe-only C5 USB bench build.
The bench check covers the distinct USB transport without rebuilding the
unchanged deck. Build output is left intact in CI so a failed toolchain or
compile step remains diagnosable.

The resulting binaries still require board, RF, storage, and power validation;
identical toolchain inputs do not constitute hardware evidence.
