#!/usr/bin/env python3
"""Keep Wi-Fi sweep's fixed metadata columns aligned before the signal meter."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "firmware-cardputer/src/ui/sweep_view.cpp"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")

    fields = ("kSweepChannelX", "kSweepBandX", "kSweepRssiX", "kSweepMeterX")
    values: dict[str, int] = {}
    for field in fields:
        match = re.search(r"constexpr\s+int\s+" + field + r"\s*=\s*(\d+)\s*;", source)
        if not match:
            print(f"FAIL: missing fixed sweep column {field}", file=sys.stderr)
            return 1
        values[field] = int(match.group(1))

    if not (96 < values["kSweepChannelX"] < values["kSweepBandX"] <
            values["kSweepRssiX"] < values["kSweepMeterX"]):
        print("FAIL: sweep metadata columns do not advance cleanly toward the meter", file=sys.stderr)
        return 1
    if values["kSweepMeterX"] + 19 > 240:
        print("FAIL: sweep meter exceeds the 240-pixel canvas", file=sys.stderr)
        return 1

    required = (
        'd.setCursor(kSweepChannelX, y);',
        'd.printf("%3u", r.ch);',
        'd.setCursor(kSweepBandX, y);',
        'd.printf("%4s", r.band5 ? "5G" : "2.4G");',
        'd.setCursor(kSweepRssiX, y);',
        'd.printf("%4d", r.rssi);',
        'drawMeter(d, kSweepMeterX, y + 1, r.rssi);',
    )
    if any(fragment not in source for fragment in required):
        print("FAIL: sweep rows do not use fixed right-aligned metadata columns", file=sys.stderr)
        return 1

    print("sweep layout test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
