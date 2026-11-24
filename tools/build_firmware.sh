#!/usr/bin/env bash
# build_firmware.sh — build both firmwares. Needs the real toolchains.
set -euo pipefail
cd "$(dirname "$0")/.."

# shellcheck disable=SC1091
source tools/env.sh

EXPECTED_IDF_VERSION="5.5.1"
EXPECTED_PIO_VERSION="6.2.0"
IDF_PATCH="$PWD/tools/idf-patches/v5.5.1-esp32c5-usb-reset-IDFGH-17050.patch"
# v5.5.1 needs Espressif's IDFGH-17050 fix (in v5.5.5+): without it a C5 rev
# v1.0 with the USB Serial/JTAG console parks in ROM when esptool resets it,
# so every flash needs BOOT held at power-on. AGENTS.md gotcha 10. The patch
# makes the tree report "-dirty", which is accepted only when that patch is
# the tree's sole change: clk.c must hash to the patched file exactly.
IDF_PATCHED_CLK_SHA="44cf9386683cb9301a675bcb9a6b077c3cf2cee2"
if [ "$(git -C "$IDF_PATH" hash-object components/esp_system/port/soc/esp32c5/clk.c)" != "$IDF_PATCHED_CLK_SHA" ]; then
    echo "ESP-IDF lacks the IDFGH-17050 fix; run: git -C \"\$IDF_PATH\" apply $IDF_PATCH" >&2
    exit 1
fi
if [ "$(git -C "$IDF_PATH" status --porcelain --untracked-files=no)" != " M components/esp_system/port/soc/esp32c5/clk.c" ] \
        || [ "$(idf.py --version)" != "ESP-IDF v$EXPECTED_IDF_VERSION-dirty" ]; then
    echo "wrong ESP-IDF: expected v$EXPECTED_IDF_VERSION plus only the IDFGH-17050 patch" >&2
    exit 1
fi
if [ "$(pio --version)" != "PlatformIO Core, version $EXPECTED_PIO_VERSION" ]; then
    echo "wrong PlatformIO Core: expected $EXPECTED_PIO_VERSION" >&2
    exit 1
fi

# --bench builds the probe to talk OCP over USB instead of the Grove UART.
# --probe-only skips the deck build; useful when checking a second probe
# transport without rebuilding the unchanged Cardputer image.
# Each variant gets its own build dir and sdkconfig: once an sdkconfig exists,
# idf.py ignores the defaults files, so a shared one would silently keep
# whichever transport was built last.
VARIANT="uart"; DEFAULTS="sdkconfig.defaults"; PROBE_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --bench)
            VARIANT="bench"; DEFAULTS="sdkconfig.defaults;sdkconfig.bench"
            ;;
        --cc1101-bench)
            # Grove OCP plus the CC1101 bench console on USB (D-15).
            VARIANT="cc1101"; DEFAULTS="sdkconfig.defaults;sdkconfig.cc1101-bench"
            ;;
        --probe-only)
            PROBE_ONLY=1
            ;;
        *)
            echo "usage: $0 [--bench|--cc1101-bench] [--probe-only]" >&2
            exit 2
            ;;
    esac
done
BUILD_DIR="build-$VARIANT"

echo "=== probe: firmware-c5 (esp32c5, $VARIANT) ==="
(
    cd firmware-c5
    idf.py -B "$BUILD_DIR" -DSDKCONFIG="$BUILD_DIR/sdkconfig" \
        -DSDKCONFIG_DEFAULTS="$DEFAULTS" -DIDF_TARGET=esp32c5 build
    grep -q "^CONFIG_OSCILLA_OCP_TRANSPORT_$([ "$VARIANT" = bench ] && echo USB || echo UART)=y" \
        "$BUILD_DIR/sdkconfig" || { echo "transport mismatch in $BUILD_DIR" >&2; exit 1; }
    if [ "$VARIANT" = cc1101 ]; then
        grep -q "^CONFIG_OSCILLA_CC1101_BENCH=y" "$BUILD_DIR/sdkconfig" \
            || { echo "CC1101 bench console missing in $BUILD_DIR" >&2; exit 1; }
    elif grep -q "^CONFIG_OSCILLA_CC1101_BENCH=y" "$BUILD_DIR/sdkconfig"; then
        echo "CC1101 bench console leaked into $BUILD_DIR" >&2; exit 1
    fi
    echo "  transport verified: $VARIANT -> firmware-c5/$BUILD_DIR"
)
if [ "$PROBE_ONLY" -eq 0 ]; then
    echo
    echo "=== deck: firmware-cardputer (esp32s3) ==="
    (
        cd firmware-cardputer
        pio run -e cardputer-adv
        python3 ../tools/check_deck_stack.py .pio/build/cardputer-adv/firmware.elf
    )
else
    echo
    echo "deck skipped (--probe-only)"
fi

echo
if [ "$PROBE_ONLY" -eq 0 ]; then
    echo "both firmwares built"
else
    echo "probe firmware built"
fi
