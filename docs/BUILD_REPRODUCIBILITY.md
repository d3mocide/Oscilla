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
is not treated as reviewable source. CI runs the same host security gate and
both C5 build variants.

The resulting binaries still require board, RF, storage, and power validation;
identical toolchain inputs do not constitute hardware evidence.
