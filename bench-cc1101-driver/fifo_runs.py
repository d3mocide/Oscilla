#!/usr/bin/env python3
"""fifo_runs.py — does CC1101 FIFO sampling over SPI recover real OOK pulses? Bench only.

Needs a probe running the `--cc1101-bench` build and a Flipper as a known
433.92 MHz Princeton source (made-up key, te=400 us). The run has three
phases:
  async          baseline RSSI floor (no FIFO traffic)
  fifo           FIFO sampling at --baud with Flipper bursts
  fifo + lload   the same with the SX1262 sweeping on the shared bus
For each burst, B-line hex is turned into bits and then into run lengths.
Princeton pulses are 1 te or 3 te, so real pulses cluster at
te*baud and 3*te*baud bits, about 10 and 30 at 25 kbaud.

  python3 fifo_runs.py --c5 <port> --flipper <port> --transmit
"""
import argparse
import collections
import statistics as st
import sys
import threading
import time

import serial

KEY, TE_US = "A5C3E1", 400


class Probe(threading.Thread):
    def __init__(self, port):
        super().__init__(daemon=True)
        self.ser = serial.Serial(port, 115200, timeout=0.2)
        self.rows, self.blocks, self.lines = [], [], []
        self.stop = threading.Event()

    def send(self, cmd):
        self.ser.write((cmd + "\n").encode())

    def run(self):
        while not self.stop.is_set():
            line = self.ser.readline().decode(errors="replace").strip()
            now = time.time()
            try:
                if line.startswith("R,"):
                    self.rows.append((now, int(line.split(",")[2])))
                elif line.startswith("B,"):
                    _, _, n, hexs = line.split(",")
                    if len(hexs) == 2 * int(n):
                        self.blocks.append((now, bytes.fromhex(hexs)))
                elif line:
                    self.lines.append((now, line))
            except ValueError:
                pass

    def wait_line(self, prefix, after, timeout):
        end = time.time() + timeout
        while time.time() < end:
            for t, line in self.lines:
                if t >= after and line.startswith(prefix):
                    return line
            time.sleep(0.05)
        return ""


class Flipper:
    def __init__(self, port):
        self.ser = serial.Serial(port, 230400, timeout=0.2)
        self.ser.write(b"\x03\r\n")
        self._prompt(3)

    def _prompt(self, timeout):
        buf, end = b"", time.monotonic() + timeout
        while time.monotonic() < end:
            buf += self.ser.read(256)
            if buf.rstrip().endswith(b">:"):
                break

    def burst(self):
        self.ser.write(f"subghz tx {KEY} 433920000 {TE_US} 10 0\r\n".encode())
        self._prompt(30)


def runs(data):
    bits = "".join(f"{b:08b}" for b in data)
    out, i = [], 0
    while i < len(bits):
        j = i
        while j < len(bits) and bits[j] == bits[i]:
            j += 1
        out.append((bits[i], j - i))
        i = j
    return out[1:-1]   # edges are cut off by the window


def near(length, target, tol):
    return abs(length - target) <= tol


def phase(probe, fz, name, cmds, seconds, bursts, baud):
    t0 = time.time()
    for c in cmds:
        probe.send(c)
        time.sleep(0.5)
    time.sleep(1.0)
    start, marks = time.time(), []
    gap = seconds / (bursts + 1) if bursts else seconds
    for _ in range(bursts):
        time.sleep(gap)
        a = time.time()
        fz.burst()
        marks.append((a, time.time()))
    time.sleep(max(0.0, seconds - (time.time() - start)))
    inside = lambda t: any(a - 0.2 <= t <= b + 0.6 for a, b in marks)
    quiet = [v for t, v in probe.rows if t >= start and not inside(t)]
    floor = st.median(quiet) if quiet else None
    line = f"{name:13} floor {floor} dBm"
    if baud:
        short, long_ = TE_US * baud / 1e6, 3 * TE_US * baud / 1e6
        tol = max(2, short * 0.3)
        hit_bursts, burst_hist = 0, collections.Counter()
        for a, b in marks:
            data = b"".join(d for t, d in probe.blocks if a - 0.2 <= t <= b + 0.6)
            r = [n for _, n in runs(data)]
            s = sum(near(n, short, tol) for n in r)
            l = sum(near(n, long_, tol * 2) for n in r)
            burst_hist.update(min(n, 99) for n in r if n >= 3)
            hit_bursts += s >= 20 and l >= 10
        qdata = b"".join(d for t, d in probe.blocks if t >= start and not inside(t))
        qr = [n for _, n in runs(qdata)]
        q_s = sum(near(n, short, tol) for n in qr)
        q_l = sum(near(n, long_, tol * 2) for n in qr)
        info = probe.wait_line("I,partnum", t0, 0.1) or ""
        line += (f"  bursts with Princeton runs {hit_bursts}/{len(marks)}"
                 f"  quiet: {len(qdata)} B, {q_s} short-like / {q_l} long-like runs")
        top = ", ".join(f"{k}:{v}" for k, v in burst_hist.most_common(8))
        line += f"\n{'':15}burst run lengths (bits:count) {top}"
    print(line, flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--c5", required=True)
    ap.add_argument("--flipper", required=True)
    ap.add_argument("--transmit", action="store_true")
    ap.add_argument("--baud", type=int, default=25000)
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--bursts", type=int, default=5)
    args = ap.parse_args()
    if not args.transmit:
        sys.exit("refusing: the Flipper transmits test bursts; pass --transmit")
    probe = Probe(args.c5)
    probe.start()
    time.sleep(5)   # opening the USB console resets the C5; let it boot
    fz = Flipper(args.flipper)
    phase(probe, fz, "async", ["lload off", "mode async"], args.seconds, 0, 0)
    phase(probe, fz, "fifo", [f"mode fifo {args.baud}"], args.seconds, args.bursts, args.baud)
    phase(probe, fz, "fifo + lload", ["lload on"], args.seconds, args.bursts, args.baud)
    t = time.time()
    probe.send("i")
    time.sleep(1)
    info = probe.wait_line("I,partnum", t, 2)
    keys = ("fifo_baud", "fifo_bytes", "fifo_overflows", "fifo_errors", "fifo_dropped", "fifo_late", "drifts", "lload_sweeps", "lload_errors")
    print("counters:", ", ".join(f"{k}={p.split('=')[1]}" for p in info.split(",") for k in keys
                                  if p.startswith(k + "=")))
    probe.send("lload off")
    probe.send("mode async")
    time.sleep(1)
    for t, line in probe.lines:
        if line.startswith("E,"):
            print("event:", line)
    probe.stop.set()


if __name__ == "__main__":
    main()
