#!/usr/bin/env python3
"""legacy_bench.py — bench test sequence for the CC1101 ("legacy") lane.

Talks to the deck's own USB debug console (plain text lines, gated on debug
mode — press 'd' on the physical keyboard first if nothing comes back), not
the probe's OCP link directly. This automates the exact by-hand sequence
used to validate the CC1101 driver and the subghz_arbiter/LoRa exclusion on
2026-09-21 (see WORKLOG), so the next bench session doesn't retype it.

Requires pyserial (a live port is the whole point of this tool; unlike
ocp.py/ocp_repl.py, there is no --replay/--selftest mode here since there is
nothing to replay against — the debug console has no canned fixture).

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import argparse
import re
import sys
import time

try:
    import serial  # type: ignore
except ImportError:
    print("pyserial is required: pip install pyserial", file=sys.stderr)
    sys.exit(1)

DEFAULT_FREQ_HZ = 433920000   # common US weather-sensor frequency, a starting point


def send(ser: "serial.Serial", line: str, wait_s: float) -> str:
    ser.write((line + "\n").encode())
    time.sleep(wait_s)
    return ser.read(65536).decode(errors="replace")


def parse_kv(text: str, marker: str) -> dict[str, str]:
    """Pulls k=v pairs off the last line containing `marker` (e.g. "dump")."""
    line = ""
    for candidate in text.splitlines():
        if marker in candidate:
            line = candidate
    return dict(re.findall(r"(\w+)=(-?\S*)", line))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("port", help="deck's serial port, e.g. /dev/ttyACM0 or a /dev/serial/by-id/... path")
    ap.add_argument("--freq", type=int, default=DEFAULT_FREQ_HZ, help=f"legacy_config frequency in Hz (default {DEFAULT_FREQ_HZ})")
    ap.add_argument("--duration", type=float, default=20.0, help="capture window in seconds (default 20)")
    ap.add_argument("--arbiter", action="store_true", help="also run the LoRa/CC1101 mutual-exclusion cross-check")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    time.sleep(0.3)
    ser.read(65536)   # drop whatever was already buffered

    print("=== connectivity + debug-mode check ===")
    out = send(ser, "dump", 1.0)
    baseline = parse_kv(out, "dump")
    if not baseline:
        print("No response to `dump`. Debug mode is probably off — press 'd' on the")
        print("physical keyboard, then re-run this script. (Debug mode persists across")
        print("reboots once set, but not across a fresh flash that resets storage.)")
        return 1
    print(f"link={baseline.get('link', '?')}  probe_heap={baseline.get('probe_heap', '?')}")

    print("\n=== CC1101 hardware-alive check ===")
    out = send(ser, "legacy id", 1.5)
    m = re.search(r"legacy id partnum=(\d+) chipver=(\d+)", out)
    if not m:
        print("No `legacy id` response — check the probe is flashed with the uart/prod")
        print("build and Grove is connected. Raw output:")
        print(out)
        return 1
    partnum, chipver = m.group(1), m.group(2)
    print(f"partnum={partnum} chipver={chipver}", end="  ")
    print("(matches the documented genuine-CC1101 signature)" if partnum == "0" and chipver == "20"
          else "(does NOT match the 0/20 signature seen 2026-09-21 — new chip, or something's wrong)")

    print(f"\n=== capture: {args.freq} Hz for {args.duration:.0f}s ===")
    send(ser, "legacy config", 1.0)
    send(ser, "legacy", 1.0)   # toggle: starts listening
    time.sleep(args.duration)
    out = send(ser, "dump", 1.0)
    after = parse_kv(out, "dump")
    send(ser, "legacy", 1.0)   # toggle: stops listening

    pkts = int(after.get("legacy_pkts", 0)) - int(baseline.get("legacy_pkts", 0))
    rssi = after.get("legacy_rssi", "?")
    rate = pkts / args.duration if args.duration > 0 else 0
    print(f"events={pkts}  last_rssi={rssi}  rate={rate:.2f}/s")
    if rate > 5:
        print("Rate looks continuous, not bursty — probably still noise, not real")
        print("device traffic (see WORKLOG 2026-09-21's research: real 433 MHz devices")
        print("burst for ms at a time with multi-second-to-hour gaps). Squelch may need")
        print("more tuning (AGCCTRL1 threshold), or nothing real is on-air right now.")
    elif pkts == 0:
        print("Zero events — either nothing transmitted in this window (plausible,")
        print("real devices are infrequent), or the squelch is gating too hard.")
    else:
        print("Sparse and bursty — consistent with real device traffic. Worth pulling")
        print("the SD log (/oscilla/legacy/) to look at the actual payload bytes.")

    if args.arbiter:
        print("\n=== arbiter cross-check (LoRa vs CC1101 mutual exclusion) ===")
        send(ser, "lora config", 1.0)
        send(ser, "lora", 1.0)
        time.sleep(1)
        out = send(ser, "legacy", 1.0)
        lora_blocks_legacy = "code=busy" in out
        send(ser, "lora", 1.0)   # stop LoRa
        time.sleep(1)
        send(ser, "legacy config", 1.0)
        send(ser, "legacy", 1.0)
        time.sleep(1)
        out = send(ser, "lora", 1.0)
        legacy_blocks_lora = "code=busy" in out
        send(ser, "legacy", 1.0)   # stop CC1101

        print(f"LoRa active -> CC1101 refused: {'PASS' if lora_blocks_legacy else 'FAIL'}")
        print(f"CC1101 active -> LoRa refused: {'PASS' if legacy_blocks_lora else 'FAIL'}")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
