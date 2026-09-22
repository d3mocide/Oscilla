#!/usr/bin/env python3
"""legacy_bench.py — bench test sequence for the CC1101 ("legacy") lane.

Talks to the deck's own USB debug console (plain text lines, gated on debug
mode — press 'd' on the physical keyboard first if nothing comes back), not
the probe's OCP link directly. This automates the exact by-hand sequence
used to validate the CC1101 driver and the subghz_arbiter/LoRa exclusion on
2026-09-21 (see WORKLOG), so the next bench session doesn't retype it.

The debug console's `legacy config` command has no frequency argument today
— deck_app.cpp's startLegacyConfig() always sends the hardcoded
kLegacyBenchFreqHz (433.92 MHz). There is deliberately no --freq flag here:
one that looked like it worked but silently did nothing would be worse than
none (caught in review 2026-09-21 — the first version of this script had
exactly that bug).

--selftest checks the output-parsing logic against fixed sample strings
captured from a real bench session — no hardware or pyserial needed, same
shape as ocp_repl.py's --selftest.

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import argparse
import re
import sys
import time

BENCH_FREQ_HZ = 433920000   # matches deck_app.cpp's kLegacyBenchFreqHz


def parse_kv(text: str, marker: str) -> dict[str, str]:
    """Pulls k=v pairs off the last line containing `marker` (e.g. "dump")."""
    line = ""
    for candidate in text.splitlines():
        if marker in candidate:
            line = candidate
    return dict(re.findall(r"(\w+)=(-?\S*)", line))


def parse_legacy_id(text: str) -> tuple[str, str] | None:
    m = re.search(r"legacy id partnum=(\d+) chipver=(\d+)", text)
    return (m.group(1), m.group(2)) if m else None


def run_selftest() -> int:
    checks: list[tuple[bool, str]] = []

    def check(ok: bool, what: str) -> None:
        checks.append((ok, what))
        print(f"  {'PASS' if ok else 'FAIL'}  {what}")

    # Captured verbatim from the 2026-09-21 bench session (WORKLOG).
    sample_dump = (
        "deck gnss searching age_ms=0 chkfail=0 overlong=0\n"
        "deck dump link=ready scan_rows=0 clients=0 probes=0 spectrum=0 zig_pans=0 "
        "zig_nodes=0 zig_active=0 lora_pkts=1 legacy_pkts=451 legacy_rssi=-65 "
        "deauth_evt=0 bt_devices=0 bt_trackers=0 airtag_sightings=0 anti_alerts=0 "
        "anti_starting=0 probe_heap=99948 probe_heap_total=226784 probe_heap_min=95388 "
        "probe_heap_largest=77824 probe_psram_total=8388608 probe_psram_free=8350756 "
        "probe_psram_largest=0 gnss=searching wardrive_open=0\n"
    )
    d = parse_kv(sample_dump, "dump")
    check(d.get("legacy_pkts") == "451", "parse_kv reads legacy_pkts from a real dump line")
    check(d.get("legacy_rssi") == "-65", "parse_kv reads a negative value (legacy_rssi)")
    check(d.get("link") == "ready", "parse_kv reads link=ready")
    check(parse_kv("", "dump") == {}, "parse_kv on empty input (debug mode off) returns empty, not an exception")

    sample_id = "deck debug: ok legacy id\ndeck legacy id partnum=0 chipver=20\n"
    check(parse_legacy_id(sample_id) == ("0", "20"), "parse_legacy_id reads the real partnum/chipver line")
    check(parse_legacy_id("deck debug: ok legacy\n") is None, "parse_legacy_id returns None on an unrelated reply")

    fails = sum(1 for ok, _ in checks if not ok)
    print(f"\n{'legacy_bench selftest FAILED' if fails else 'legacy_bench selftest OK'}: "
          f"{len(checks) - fails} passed, {fails} failed")
    return 1 if fails else 0


def send(ser, line: str, wait_s: float) -> str:
    ser.write((line + "\n").encode())
    time.sleep(wait_s)
    return ser.read(65536).decode(errors="replace")


def ensure_stopped(ser, cmd: str, out: str) -> None:
    """If `cmd` unexpectedly started something instead of being refused (i.e. the
    arbiter under test has a bug), stop it before the next step runs — otherwise
    a broken arbiter corrupts every step after it instead of just failing the one
    check it broke (found in review 2026-09-21)."""
    if "code=busy" not in out:
        send(ser, cmd, 1.0)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", nargs="?", help="deck's serial port, e.g. /dev/ttyACM0 or a /dev/serial/by-id/... path")
    ap.add_argument("--duration", type=float, default=20.0, help="capture window in seconds (default 20)")
    ap.add_argument("--arbiter", action="store_true", help="also run the LoRa/CC1101 mutual-exclusion cross-check")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--selftest", action="store_true", help="check the parsing logic against fixed samples; no hardware or pyserial needed")
    args = ap.parse_args()

    if args.selftest:
        return run_selftest()

    if not args.port:
        ap.error("port is required unless --selftest")

    try:
        import serial  # type: ignore
    except ImportError:
        print("pyserial is required for a live port: pip install pyserial", file=sys.stderr)
        return 1

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
    ident = parse_legacy_id(out)
    if not ident:
        print("No `legacy id` response — check the probe is flashed with the uart/prod")
        print("build and Grove is connected. Raw output:")
        print(out)
        return 1
    partnum, chipver = ident
    print(f"partnum={partnum} chipver={chipver}", end="  ")
    print("(matches the documented genuine-CC1101 signature)" if partnum == "0" and chipver == "20"
          else "(does NOT match the 0/20 signature seen 2026-09-21 — new chip, or something's wrong)")

    print(f"\n=== capture: {BENCH_FREQ_HZ} Hz (fixed — see module docstring) for {args.duration:.0f}s ===")
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
        ensure_stopped(ser, "legacy", out)   # in case the refusal didn't happen (the bug this checks for)
        send(ser, "lora", 1.0)   # stop LoRa
        time.sleep(1)

        send(ser, "legacy config", 1.0)
        send(ser, "legacy", 1.0)
        time.sleep(1)
        out = send(ser, "lora", 1.0)
        legacy_blocks_lora = "code=busy" in out
        ensure_stopped(ser, "lora", out)
        send(ser, "legacy", 1.0)   # stop CC1101

        print(f"LoRa active -> CC1101 refused: {'PASS' if lora_blocks_legacy else 'FAIL'}")
        print(f"CC1101 active -> LoRa refused: {'PASS' if legacy_blocks_lora else 'FAIL'}")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
