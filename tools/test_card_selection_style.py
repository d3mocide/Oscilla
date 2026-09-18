#!/usr/bin/env python3
"""Keep selected data rows on the Link card's rail-only visual language."""

from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
ROW_VIEWS = (
    "anti_surveillance_view.cpp",
    "bt_view.cpp",
    "contacts_view.cpp",
    "deauth_view.cpp",
    "mesh_view.cpp",
    "subghz_view.cpp",
    "sweep_view.cpp",
)


def fail(message: str) -> int:
    print(f"FAIL: {message}", file=sys.stderr)
    return 1


def main() -> int:
    for name in ROW_VIEWS:
        source = (ROOT / "firmware-cardputer/src/ui" / name).read_text(encoding="utf-8")
        if "sel ? '>'" in source:
            return fail(f"{name} still draws a text-caret selection marker")
        if "d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);" not in source:
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

    print("card selection style test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
