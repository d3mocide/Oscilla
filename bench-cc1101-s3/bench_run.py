#!/usr/bin/env python3
"""bench_run.py — drive the D-15 CC1101 requalification bench. NOT Oscilla.

  probe : read the S3 sketch (chip ID, tuning, baseline RSSI) and query the
          Flipper's firmware and `subghz` usage. Never transmits.
  run   : the Flipper sends short test bursts (`subghz tx`, internal CC1101)
          while the S3's RSSI is logged; reports per-burst peak vs baseline.
          Requires --transmit. Use a made-up key, never a real device's code.

Output goes to out/<timestamp>/ (gitignored: field data is never committed).
Requires pyserial. Address ports by /dev/serial/by-id (AGENTS gotcha 9).
"""
import argparse
import csv
import statistics
import sys
import threading
import time
from pathlib import Path

import serial

PROMPT = b">: "


class S3Reader(threading.Thread):
    """Collects R/I/D/E lines from the sketch with host receive timestamps."""

    def __init__(self, port):
        super().__init__(daemon=True)
        self.ser = serial.Serial(port, 115200, timeout=0.2)
        self.rows = []      # (host_t, t_ms, rssi_max, rssi_mean, edges)
        self.info = []
        self.stop = threading.Event()

    def send(self, cmd):
        self.ser.write((cmd + "\n").encode())

    def run(self):
        while not self.stop.is_set():
            line = self.ser.readline().decode(errors="replace").strip()
            if not line:
                continue
            now = time.monotonic()
            if line.startswith("R,"):
                try:
                    _, t, mx, mean, edges = line.split(",")
                    self.rows.append((now, int(t), int(mx), int(mean), int(edges)))
                except ValueError:
                    pass
            elif line[:2] in ("I,", "D,", "E,"):
                self.info.append(line)


class Flipper:
    def __init__(self, port):
        self.ser = serial.Serial(port, 230400, timeout=0.2)
        self.ser.write(b"\x03\r\n")
        self.read_until_prompt(3.0)

    def read_until_prompt(self, timeout):
        buf = b""
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            buf += self.ser.read(256)
            if buf.rstrip().endswith(PROMPT.strip()):
                break
        return buf.decode(errors="replace")

    def cmd(self, text, timeout=5.0):
        self.ser.write(text.encode() + b"\r\n")
        out = self.read_until_prompt(timeout)
        lines = out.splitlines()
        if lines and lines[0].strip() == text:
            lines = lines[1:]
        return "\n".join(l for l in lines if l.strip() != PROMPT.strip().decode())


def stats(rows):
    if not rows:
        return "no RSSI rows"
    mx = [r[2] for r in rows]
    edges = [r[4] for r in rows]
    return (f"{len(rows)} windows, rssi_max median {statistics.median(mx)} "
            f"min {min(mx)} max {max(mx)}, gdo0 edges/window median {statistics.median(edges)}")


def cmd_probe(args):
    s3 = S3Reader(args.s3)
    s3.start()
    s3.send("i")
    s3.send("d")
    time.sleep(args.seconds)
    s3.stop.set()
    print("== S3 / CC1101")
    for line in s3.info:
        print(" ", line)
    print("  baseline:", stats(s3.rows))
    if args.flipper:
        fz = Flipper(args.flipper)
        print("== Flipper device_info")
        info = fz.cmd("device_info")
        for line in info.splitlines():
            if any(k in line for k in ("firmware_version", "firmware_origin", "hardware_region",
                                        "firmware_commit", "firmware_branch")):
                print(" ", line.strip())
        print("== Flipper `subghz` usage")
        print(fz.cmd("subghz"))


def cmd_run(args):
    if not args.transmit:
        sys.exit("refusing: `run` transmits from the Flipper; pass --transmit after reviewing the plan")
    plan = (f"subghz tx {args.key} {args.freq_hz} {args.te} {args.repeat} 0  "
            f"x {args.bursts} bursts, {args.gap}s apart")
    print("plan:", plan)
    out = Path(__file__).parent / "out" / time.strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True)
    s3 = S3Reader(args.s3)
    s3.start()
    s3.send("i")
    fz = Flipper(args.flipper)
    time.sleep(args.gap)                     # pre-burst baseline
    bursts = []
    for i in range(args.bursts):
        t0 = time.monotonic()
        reply = fz.cmd(f"subghz tx {args.key} {args.freq_hz} {args.te} {args.repeat} 0", timeout=30)
        t1 = time.monotonic()
        bursts.append((i, t0, t1, reply.strip().replace("\n", " | ")))
        print(f"burst {i}: {t1 - t0:.2f}s  flipper: {bursts[-1][3][:80]}")
        time.sleep(args.gap)
    s3.stop.set()

    with open(out / "rssi.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["host_t", "t_ms", "rssi_max", "rssi_mean", "gdo0_edges"])
        w.writerows(s3.rows)
    in_burst = lambda t: any(b[1] - 0.05 <= t <= b[2] + 0.3 for b in bursts)
    base = [r for r in s3.rows if not in_burst(r[0])]
    base_max = statistics.median(r[2] for r in base) if base else None
    base_edges = statistics.median(r[4] for r in base) if base else None
    summary = [f"plan: {plan}", "info: " + " ".join(s3.info[:1]),
               f"baseline: {stats(base)}"]
    for i, t0, t1, reply in bursts:
        rows = [r for r in s3.rows if t0 - 0.05 <= r[0] <= t1 + 0.3]
        if not rows or base_max is None:
            summary.append(f"burst {i}: no rows")
            continue
        peak = max(r[2] for r in rows)
        edges = max(r[4] for r in rows)
        summary.append(f"burst {i}: rssi peak {peak} dBm (delta {peak - base_max:+}) "
                       f"gdo0 edges max {edges} (baseline {base_edges})")
    (out / "summary.txt").write_text("\n".join(summary) + "\n")
    print("\n".join(summary))
    print("saved", out)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--s3", required=True, help="/dev/serial/by-id/... of the XIAO S3")
    p.add_argument("--flipper", help="/dev/serial/by-id/... of the Flipper")
    sub = p.add_subparsers(dest="cmd", required=True)
    pr = sub.add_parser("probe")
    pr.add_argument("--seconds", type=float, default=5.0)
    r = sub.add_parser("run")
    r.add_argument("--transmit", action="store_true")
    r.add_argument("--key", default="A5C3E1", help="made-up 3-byte Princeton key (hex)")
    r.add_argument("--freq-hz", type=int, default=433920000)
    r.add_argument("--te", type=int, default=400)
    r.add_argument("--repeat", type=int, default=10)
    r.add_argument("--bursts", type=int, default=5)
    r.add_argument("--gap", type=float, default=3.0)
    args = p.parse_args()
    if args.cmd == "run" and not args.flipper:
        p.error("run needs --flipper")
    {"probe": cmd_probe, "run": cmd_run}[args.cmd](args)


if __name__ == "__main__":
    main()
