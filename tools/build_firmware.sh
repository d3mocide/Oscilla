#!/usr/bin/env bash
# build_firmware.sh — build both firmwares. Needs the real toolchains.
set -euo pipefail
cd "$(dirname "$0")/.."

# shellcheck disable=SC1091
source tools/env.sh

echo "=== probe: firmware-c5 (esp32c5) ==="
(
    cd firmware-c5
    # Regenerates sdkconfig, so only run it when the target is wrong.
    if ! grep -q '^CONFIG_IDF_TARGET="esp32c5"' sdkconfig 2>/dev/null; then
        idf.py set-target esp32c5 >/dev/null
    fi
    idf.py build 2>&1 | tail -3
)

echo
echo "=== deck: firmware-cardputer (esp32s3) ==="
(
    cd firmware-cardputer
    pio run 2>&1 | tail -5
)

echo
echo "both firmwares built"
