#!/usr/bin/env python3
"""legacy_pulse.py — reconstruct pulse widths from CC1101 raw envelope captures.

CC1101's synchronous OOK demodulator samples at a fixed rate
(cc1101_radio.c's TARGET_BAUD_HZ); it doesn't decode PWM-encoded devices
(most cheap 433 MHz sensors) natively the way a real edge-timestamping
receiver would. This reconstructs approximate pulse widths from the raw
bitstream by counting consecutive equal-bit run-lengths and multiplying by
the sample period — the same principle rtl_433 uses with real timestamps,
applied to periodically-sampled data instead.

Reads a legacy_logger.cpp session CSV (ts_ms,freq_hz,rssi,len,hex) from the
deck's SD card and reports the detected pulse-width histogram, to compare
against a known device's real parameters (e.g. from an independent SDR
capture) — see WORKLOG 2026-09-22 for the LaCrosse-TX141THBv2 cross-
reference this was built for (short_width=232us, long_width=460us,
sync_width=712us, measured by rtl_433).

--selftest checks the reconstruction logic against synthetic data — no CSV
or hardware needed, same shape as ocp_repl.py's/legacy_bench.py's --selftest.

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections import Counter


def bytes_to_bits(data: bytes) -> list[int]:
    """MSB-first per byte, matching CC1101's synchronous serial bit order."""
    bits = []
    for b in data:
        for i in range(7, -1, -1):
            bits.append((b >> i) & 1)
    return bits


def run_lengths(bits: list[int]) -> list[tuple[int, int]]:
    """[(level, run_length_in_bits), ...] for consecutive equal-bit runs."""
    if not bits:
        return []
    runs = []
    level = bits[0]
    length = 1
    for b in bits[1:]:
        if b == level:
            length += 1
        else:
            runs.append((level, length))
            level = b
            length = 1
    runs.append((level, length))
    return runs


def pulse_widths_us(hex_str: str, bit_period_us: float) -> list[float]:
    data = bytes.fromhex(hex_str)
    bits = bytes_to_bits(data)
    return [length * bit_period_us for _level, length in run_lengths(bits)]


def run_selftest() -> int:
    checks: list[tuple[bool, str]] = []

    def check(ok: bool, what: str) -> None:
        checks.append((ok, what))
        print(f"  {'PASS' if ok else 'FAIL'}  {what}")

    check(bytes_to_bits(b"\x00") == [0] * 8, "bytes_to_bits: 0x00 is eight 0 bits")
    check(bytes_to_bits(b"\xff") == [1] * 8, "bytes_to_bits: 0xff is eight 1 bits")
    check(bytes_to_bits(b"\x80") == [1, 0, 0, 0, 0, 0, 0, 0], "bytes_to_bits: 0x80 is MSB-first")

    check(run_lengths([0, 0, 0, 1, 1, 0]) == [(0, 3), (1, 2), (0, 1)], "run_lengths: basic run detection")
    check(run_lengths([]) == [], "run_lengths: empty input")
    check(run_lengths([1, 1, 1, 1]) == [(1, 4)], "run_lengths: single run")

    # Synthetic LaCrosse-like pulse train at the real profile's bit period
    # (38.48us at 25985 baud): a 6-bit run (232us short pulse) then a
    # 12-bit run (460us long pulse) — the exact oversampling ratio
    # cc1101_radio.c's profile was chosen to produce.
    bit_period = 1e6 / 25985
    synthetic_bits = [1] * 6 + [0] * 12
    widths = [length * bit_period for _level, length in run_lengths(synthetic_bits)]
    check(abs(widths[0] - 232) < 5, f"synthetic 6-bit run reconstructs to ~232us (got {widths[0]:.1f})")
    check(abs(widths[1] - 460) < 10, f"synthetic 12-bit run reconstructs to ~460us (got {widths[1]:.1f})")

    fails = sum(1 for ok, _ in checks if not ok)
    print(f"\n{'legacy_pulse selftest FAILED' if fails else 'legacy_pulse selftest OK'}: "
          f"{len(checks) - fails} passed, {fails} failed")
    return 1 if fails else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv_path", nargs="?", help="legacy session CSV from /oscilla/legacy/ on the deck's SD card")
    ap.add_argument("--baud", type=float, default=25985,
                     help="achieved DRATE in baud (default: 25985, cc1101_radio.c's current profile)")
    ap.add_argument("--known", type=float, nargs="*", default=[232, 460, 712],
                     help="known real pulse widths in us to check for matches "
                          "(default: LaCrosse-TX141THBv2's rtl_433-measured values)")
    ap.add_argument("--tolerance", type=float, default=0.20, help="fractional match tolerance (default 0.20 = +-20%%)")
    ap.add_argument("--selftest", action="store_true", help="check the reconstruction logic against synthetic data")
    args = ap.parse_args()

    if args.selftest:
        return run_selftest()
    if not args.csv_path:
        ap.error("csv_path is required unless --selftest")

    bit_period_us = 1e6 / args.baud
    all_widths: list[float] = []
    with open(args.csv_path, newline="") as f:
        for row in csv.DictReader(f):
            hex_str = row.get("hex", "")
            if hex_str:
                all_widths.extend(pulse_widths_us(hex_str, bit_period_us))

    if not all_widths:
        print("No pulse widths reconstructed (empty file or no hex payloads).")
        return 1

    print(f"{len(all_widths)} pulse widths reconstructed from {args.csv_path}")
    print(f"bit period: {bit_period_us:.2f}us (baud={args.baud})")

    buckets = Counter(int(w // 50) * 50 for w in all_widths)
    print("\nHistogram (50us buckets):")
    for bucket in sorted(buckets):
        count = buckets[bucket]
        bar = "#" * min(count, 60)
        print(f"  {bucket:5d}-{bucket + 49:5d}us: {count:5d} {bar}")

    print(f"\nChecking against known widths {args.known}us (+-{args.tolerance * 100:.0f}%):")
    for known in args.known:
        lo, hi = known * (1 - args.tolerance), known * (1 + args.tolerance)
        matches = sum(1 for w in all_widths if lo <= w <= hi)
        pct = 100 * matches / len(all_widths)
        print(f"  {known:.0f}us: {matches} matches ({pct:.1f}% of all reconstructed pulses)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
