#!/usr/bin/env python3
"""Keep selected data rows on the Link card's rail-only visual language."""

from __future__ import annotations

from pathlib import Path
import sys
import re


ROOT = Path(__file__).resolve().parents[1]
ROW_VIEWS = (
    "anti_surveillance_view.cpp",
    "bt_view.cpp",
    "contacts_view.cpp",
    "deauth_view.cpp",
    "zig_view.cpp",
    "subghz_view.cpp",
    "sweep_view.cpp",
    "spectrum_view.cpp",
    "session_view.cpp",
)


def fail(message: str) -> int:
    print(f"FAIL: {message}", file=sys.stderr)
    return 1


def main() -> int:
    list_row = (ROOT / "firmware-cardputer/src/ui/list_row.h").read_text(encoding="utf-8")
    if "d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);" not in list_row:
        return fail("list_row.h's drawRowHighlight is missing the Link-style selection rail")

    for name in ROW_VIEWS:
        source = (ROOT / "firmware-cardputer/src/ui" / name).read_text(encoding="utf-8")
        if "sel ? '>'" in source:
            return fail(f"{name} still draws a text-caret selection marker")
        if "drawRowHighlight(d, y, sel)" not in source:
            return fail(f"{name} is missing the Link-style selection rail")
        if "d.setCursor(0, y);" in source or "d.setCursor(6, y);" not in source:
            return fail(f"{name} lets row text overwrite the selection rail")

    sniff = (ROOT / "firmware-cardputer/src/ui/contacts_view.cpp").read_text(encoding="utf-8")
    required = (
        "kSniffClientChannelX",
        "kSniffClientBandX",
        "kSniffRssiX",
        'd.printf("%4s", r.band5 ? "5G" : "2.4G");',
        'd.print("LAST ");',
    )
    if any(fragment not in sniff for fragment in required):
        return fail("Sniffer rows are missing their fixed metadata columns or LAST label")

    positions = {name: int(value) for name, value in re.findall(r"constexpr int (kSniff\w+X) = (\d+);", sniff)}
    channel, band, rssi = (positions[name] for name in ("kSniffClientChannelX", "kSniffClientBandX", "kSniffRssiX"))
    if not (6 + 17*6 <= channel and channel + 3*6 <= band and band + 4*6 <= rssi and rssi + 4*6 <= 240):
        return fail("Sniffer full MAC/channel/band/RSSI columns overlap at device font size")
    if 'printable(r.mac, 17)' not in sniff or 'ENTER DETAIL' not in sniff:
        return fail("Sniffer must show full MAC identity and its inspection action")

    print("card selection style test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
