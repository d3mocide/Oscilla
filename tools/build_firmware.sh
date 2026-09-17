#!/usr/bin/env bash
# build_firmware.sh — build both firmwares. Needs the real toolchains.
set -euo pipefail
cd "$(dirname "$0")/.."

# shellcheck disable=SC1091
source tools/env.sh

EXPECTED_IDF_VERSION="5.5.1"
EXPECTED_PIO_VERSION="6.2.0"
if [ "$(idf.py --version)" != "ESP-IDF v$EXPECTED_IDF_VERSION" ]; then
    echo "wrong ESP-IDF: expected v$EXPECTED_IDF_VERSION" >&2
    exit 1
fi
if [ "$(pio --version)" != "PlatformIO Core, version $EXPECTED_PIO_VERSION" ]; then
    echo "wrong PlatformIO Core: expected $EXPECTED_PIO_VERSION" >&2
    exit 1
fi

# --bench builds the probe to talk OCP over USB instead of the Grove UART.
# Each variant gets its own build dir and sdkconfig: once an sdkconfig exists,
# idf.py ignores the defaults files, so a shared one would silently keep
# whichever transport was built last.
VARIANT="uart"; DEFAULTS="sdkconfig.defaults"
if [ "${1:-}" = "--bench" ]; then
    VARIANT="bench"; DEFAULTS="sdkconfig.defaults;sdkconfig.bench"
fi
BUILD_DIR="build-$VARIANT"

echo "=== probe: firmware-c5 (esp32c5, $VARIANT) ==="
(
    cd firmware-c5
    idf.py -B "$BUILD_DIR" -DSDKCONFIG="$BUILD_DIR/sdkconfig" \
        -DSDKCONFIG_DEFAULTS="$DEFAULTS" -DIDF_TARGET=esp32c5 build
    grep -q "^CONFIG_OSCILLA_OCP_TRANSPORT_$([ "$VARIANT" = bench ] && echo USB || echo UART)=y" \
        "$BUILD_DIR/sdkconfig" || { echo "transport mismatch in $BUILD_DIR" >&2; exit 1; }
    echo "  transport verified: $VARIANT -> firmware-c5/$BUILD_DIR"
)
echo
echo "=== deck: firmware-cardputer (esp32s3) ==="
(
    cd firmware-cardputer
    pio run
)

echo
echo "both firmwares built"
