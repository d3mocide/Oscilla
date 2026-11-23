#!/usr/bin/env python3
"""coexist.py — CC1101/SX1262 shared-bus coexistence run (D-15). Bench only.

Talks to a probe built with `build_firmware.sh --cc1101-bench` (USB console)
and a Flipper used as a 433.92 MHz test source. In each phase, `lload` is
off or on (SX1262 receive-only RSSI sweeps on the shared SPI bus). It reports
the CC1101 floor, windows raised with no known source, Flipper burst
detection, and SX1262 sweep counts and errors. With --stress it also runs
bring-up and RX cycling under load, to exercise the bus lock.

  python3 coexist.py --c5 /dev/serial/by-id/<c5> --flipper /dev/serial/by-id/<flipper> --transmit

--transmit is required because the Flipper (not Oscilla) emits the made-up
test key. Results go to stdout; nothing is written to the repo.
"""
import argparse
import statistics as st
import sys
import threading
import time
import serial

KEY = "A5C3E1"


class Flipper:
    PROMPT = b">: "

    def __init__(self, port):
        self.ser = serial.Serial(port, 230400, timeout=0.2)
        self.ser.write(b"\x03\r\n")
        self._until_prompt(3.0)

    def _until_prompt(self, timeout):
        buf, end = b"", time.monotonic() + timeout
        while time.monotonic() < end:
            buf += self.ser.read(256)
            if buf.rstrip().endswith(self.PROMPT.strip()):
                break
        return buf.decode(errors="replace")

    def burst(self, freq_hz=433920000):
        self.ser.write(f"subghz tx {KEY} {freq_hz} 400 10 0\r\n".encode())
        return self._until_prompt(30.0)


class Probe(threading.Thread):
    def __init__(self, port):
        super().__init__(daemon=True)
        self.ser = serial.Serial(port, 115200, timeout=0.2)
        self.rows, self.lines = [], []
        self.stop = threading.Event()

    def send(self, cmd):
        self.ser.write((cmd + "\n").encode())

    def run(self):
        while not self.stop.is_set():
            line = self.ser.readline().decode(errors="replace").strip()
            if line.startswith("R,"):
                f = line.split(",")
                try:
                    self.rows.append((time.time(), int(f[2])))
                except (IndexError, ValueError):
                    pass
            elif line:
                self.lines.append((time.time(), line))

    def wait_line(self, prefix, after, timeout):
        end = time.time() + timeout
        while time.time() < end:
            for t, line in self.lines:
                if t >= after and line.startswith(prefix):
                    return line
            time.sleep(0.05)
        return None

    def info(self):
        t = time.time()
        self.send("i")
        return self.wait_line("I,partnum", t, 3) or ""


def field(line, key):
    for part in line.split(","):
        if part.startswith(key + "="):
            return part.split("=", 1)[1]
    return "?"


def phase(probe, fz, name, seconds, bursts):
    t0 = time.time()
    marks = []
    gap = seconds / (bursts + 1)
    for _ in range(bursts):
        time.sleep(gap)
        a = time.time()
        fz.burst()
        marks.append((a, time.time()))
    time.sleep(max(0.0, seconds - (time.time() - t0)))
    rows = [r for r in probe.rows if r[0] >= t0 + 1.0]
    near = lambda t: any(a - 1.0 <= t <= b + 1.5 for a, b in marks)
    quiet = [r[1] for r in rows if not near(r[0])]
    floor = st.median(quiet) if quiet else None
    p99 = sorted(quiet)[int(len(quiet) * 0.99)] if quiet else None
    raised = sum(1 for v in quiet if floor is not None and v - floor >= 8)
    hits = 0
    for a, b in marks:
        w = [r[1] for r in rows if a - 0.05 <= r[0] <= b + 0.3]
        hits += bool(w) and floor is not None and max(w) - floor >= 10
    info = probe.info()
    print(f"{name:14} floor {floor} dBm  p99 {p99}  raised-no-source {raised}/{len(quiet)}  "
          f"flipper {hits}/{len(marks)}  lload_sweeps {field(info, 'lload_sweeps')}  "
          f"lload_errors {field(info, 'lload_errors')}  drifts {field(info, 'drifts')}", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--c5", required=True)
    ap.add_argument("--flipper", required=True)
    ap.add_argument("--transmit", action="store_true")
    ap.add_argument("--seconds", type=float, default=60)
    ap.add_argument("--bursts", type=int, default=5)
    ap.add_argument("--stress", action="store_true")
    args = ap.parse_args()
    if not args.transmit:
        sys.exit("refusing: the Flipper transmits test bursts; pass --transmit")

    probe = Probe(args.c5)
    probe.start()
    time.sleep(1.0)
    print("bring-up:", probe.info().split("driver=oscilla-c5,")[-1], flush=True)
    fz = Flipper(args.flipper)

    probe.send("lload off")
    time.sleep(1.0)
    phase(probe, fz, "lload off", args.seconds, args.bursts)
    t = time.time()
    probe.send("lload on")
    print("  ", probe.wait_line("I,lload", t, 3) or probe.wait_line("E,lload", t, 1), flush=True)
    phase(probe, fz, "lload ON", args.seconds, args.bursts)
    if args.stress:
        for cmd in ("cycle 500", "rxcyc 2000"):
            t = time.time()
            probe.send(cmd)
            print(f"  {cmd} under load:", probe.wait_line("S,", t, 120), flush=True)
    probe.send("lload off")
    time.sleep(1.0)
    phase(probe, fz, "lload off", args.seconds, args.bursts)
    for t, line in probe.lines:
        if line.startswith("E,"):
            print("  event:", line)
    probe.stop.set()


if __name__ == "__main__":
    main()
