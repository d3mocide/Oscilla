#!/usr/bin/env bash
# flash_c5.sh — flash the C5 over its native USB Serial/JTAG port.
#
# USB reset modes are intentional: default_reset/hard_reset can leave an
# ESP32-C5 in ROM download mode after a manual BOOT entry. See
# docs/hardware/link-bringup.md and Espressif's C5 esptool notes.

set -euo pipefail
cd "$(dirname "$0")/.."

# shellcheck disable=SC1091
source tools/env.sh

variant="uart"
loader_mode=0
while [ "$#" -gt 0 ]; do
    case "$1" in
    --bench) variant="bench"; shift ;;
    --uart)  variant="uart"; shift ;;
    --loader) loader_mode=1; shift ;;
    --help|-h)
        printf 'usage: %s [--bench|--uart] [--loader] [PORT]\n' "$0"
        printf '  --bench  flash firmware-c5/build-bench (USB OCP)\n'
        printf '  --uart   flash firmware-c5/build-uart (Grove OCP, default)\n'
        printf '  --loader use when the C5 is already in ROM loader mode\n'
        exit 0
        ;;
    *) break ;;
    esac
done

if [ "$#" -gt 1 ]; then
    echo "usage: $0 [--bench|--uart] [--loader] [PORT]" >&2
    exit 2
fi

port="${1:-${C5_PORT:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_38:44:BE:BF:D2:94-if00}}"
build="firmware-c5/build-$variant"

for image in bootloader/bootloader.bin partition_table/partition-table.bin oscilla-c5.bin; do
    if [ ! -f "$build/$image" ]; then
        echo "missing $build/$image; run ./tools/build_firmware.sh --$variant first" >&2
        exit 1
    fi
done

flash_args=(
    --chip esp32c5
    --port "$port"
    --baud 460800
    --no-stub write_flash
    --flash_mode dio
    --flash_size 8MB
    --flash_freq 80m
    0x2000 "$build/bootloader/bootloader.bin"
    0x8000 "$build/partition_table/partition-table.bin"
    0x10000 "$build/oscilla-c5.bin"
)

if [ "$loader_mode" -eq 1 ]; then
    python -m esptool --before no_reset --after watchdog_reset "${flash_args[@]}"
    exit $?
fi

# usb_reset is the normal path from a running app. Hard reset leaves this C5's
# USB bench application usable after that path. If the reset has already
# landed in ROM but the first sync races USB re-enumeration, no_reset reconnects
# to the loader and watchdog-reset is the full reset needed to escape a
# manually-entered loader.
if ! python -m esptool --before usb_reset --after hard_reset "${flash_args[@]}"; then
    echo "USB reset handshake missed; retrying an already-open ROM loader..." >&2
    python -m esptool --before no_reset --after watchdog_reset "${flash_args[@]}"
fi
