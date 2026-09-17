# Reproducible build inputs

Oscilla's local build gate is intentionally strict about the toolchains that
own hardware behavior:

- ESP-IDF `v5.5.1` for the ESP32-C5 probe;
- PlatformIO Core `6.2.0` for the Cardputer ADV deck;
- PlatformIO platform `espressif32@7.0.1`;
- M5Unified `0.2.21`, M5GFX `0.2.28`, and M5Cardputer `1.1.1` in
  `firmware-cardputer/platformio.ini`.

`tools/build_firmware.sh` asserts the first two versions before either board
build starts. The platform and library versions are declared in
`platformio.ini`; generated `.pio` package state remains a build artifact and
is not treated as reviewable source. CI installs PlatformIO into a dedicated
Python virtualenv so the pinned `pio` executable is on PATH independently of
the runner's user-install layout. CI deliberately adds that virtualenv to
`PATH` only after ESP-IDF has created its own Python environment; otherwise the
ESP-IDF installer rejects the nested virtualenv. The resulting path is written
to the runner environment from a step, rather than using the unavailable
`runner` context at job-level `env`. CI then runs the same host security gate
and both C5 build variants. Build output is left intact in CI so a failed
toolchain or compile step remains diagnosable.

The resulting binaries still require board, RF, storage, and power validation;
identical toolchain inputs do not constitute hardware evidence.
