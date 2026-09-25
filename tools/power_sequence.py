#!/usr/bin/env python3
"""power_sequence.py — walk the P6 state matrix through bench-power-meter.

    power_sequence.py --selftest
    power_sequence.py /dev/serial/by-id/<xiao-s3> --rail 3v3 --shunt-mohm 50 [--dwell 20]
    power_sequence.py /dev/serial/by-id/<xiao-s3> --monitor --shunt-mohm 50

The meter's USB carries both the probe's OCP link and the current readings;
this script drives the probe through docs/hardware/power-budget.md §6, marks
each transition on the meter's clock, and writes per-state figures.

Writes only meter records, marks, and status owner/lora — never probe frame
contents, which can carry real SSIDs (SECURITY.md).

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ocp import (  # noqa: E402
    Error, Frame, Pong, encode_command, read_header_defines, HEADER_PATH,
)
from power_meter import Demux, Meter, summarize, WIN_INT_FIELDS  # noqa: E402

REPO = Path(__file__).resolve().parent.parent

# power-budget.md §6. Each state: (label, commands). Probe verbs only.
def state_matrix(lora_cfg: str) -> list[tuple[str, list[str]]]:
    return [
        ("s01_idle",        ["stop"]),
        ("s02_scan",        ["scan_networks"]),
        ("s03_wifi_2g4_ch6", ["packet_monitor 6"]),
        ("s04_wifi_5g_ch36", ["stop phy", "packet_monitor 36"]),
        ("s05_wifi_sniff",  ["stop phy", "start_sniffer"]),
        ("s06_lora_only",   ["stop", f"lora_config {lora_cfg}", "lora_listen"]),
        ("s07_both",        ["start_sniffer"]),
        ("s08_lora_again",  ["stop phy"]),
        ("s09_wifi_again",  ["start_sniffer", "stop lora"]),
        ("s10_idle_end",    ["stop"]),
    ]


# Rails the meter can sit on, and the sag threshold that suits each.
RAILS = {"3v3": 3150, "5v": 4500}


def reply_markers() -> dict[str, str]:
    """verb -> reply marker, from OCP_VERB_TABLE (ocp.h owns the literals)."""
    src = HEADER_PATH.read_text(encoding="utf-8")
    table = src.split("#define OCP_VERB_TABLE(X)", 1)[1]
    d = read_header_defines()
    out = {}
    for m in re.finditer(r"^\s*X\(\w+,\s*(\w+),\s*\w+,\s*\d+,\s*\d+,\s*(\"\"|\w+)\)", table, re.M):
        out[d[m.group(1)]] = "" if m.group(2) == '""' else d[m.group(2)]
    return out


class Link:
    """The meter's serial port: a reader thread feeding one queue."""

    def __init__(self, port: str) -> None:
        import queue
        import threading
        import serial  # type: ignore

        self.ser = serial.Serial(port, 115200, timeout=0.05)
        self.q: "queue.Queue" = queue.Queue()
        self.windows: list[Meter] = []
        self.marks: list[Meter] = []
        self.probe_events: list[Meter] = []
        self._demux = Demux()
        self._stop = threading.Event()
        self._t = threading.Thread(target=self._read, daemon=True)
        self._t.start()
        self.ser.write(b"\n")   # end any half-line left in the meter

    def _read(self) -> None:
        while not self._stop.is_set():
            data = self.ser.read(512)
            for item in self._demux.feed(data):
                self.q.put(item)

    def meter(self, cmd: str) -> None:
        self.ser.write(f"#{cmd}\n".encode("ascii"))

    def probe(self, cmdline: str) -> None:
        parts = cmdline.split()
        self.ser.write(encode_command(parts[0], *parts[1:]))

    def wait(self, pred, timeout: float):
        """Record every meter item; return the first item matching pred."""
        import queue
        deadline = time.time() + timeout
        while True:
            left = deadline - time.time()
            if left <= 0:
                return None
            try:
                item = self.q.get(timeout=min(left, 0.1))
            except queue.Empty:
                continue
            if isinstance(item, Meter):
                {"win": self.windows, "mark": self.marks,
                 "probe": self.probe_events}.get(item.kind, []).append(item)
            if pred(item):
                return item

    def close(self) -> None:
        self._stop.set()
        self._t.join(timeout=1.0)
        self.ser.close()


def meter_ok(link: Link, cmd: str) -> bool:
    link.meter(cmd)
    r = link.wait(lambda i: isinstance(i, Meter) and i.kind in ("ok", "err"), 2.0)
    return r is not None and r.kind == "ok"


def run_live(args) -> int:
    markers = reply_markers()
    link = Link(args.port)
    errors: dict[str, list[str]] = {}
    statuses: dict[str, str] = {}
    try:
        link.meter("stream off")
        link.wait(lambda i: False, 0.3)
        link.meter("info")
        info = link.wait(lambda i: isinstance(i, Meter) and i.kind == "info", 2.0)
        if info is None:
            print("meter did not answer #info — wrong port, or not the meter firmware?")
            return 1
        if info.kv.get("ina") not in ("ina226", "ina228"):
            print(f"meter reports ina={info.kv.get('ina')} — check the INA226 wiring (README)")
            return 1
        shunt_uohm = round(args.shunt_mohm * 1000)
        for cmd in (f"window {args.window_ms}", f"shunt {shunt_uohm}", f"sag {RAILS[args.rail]}"):
            if not meter_ok(link, cmd):
                print(f"meter refused '#{cmd}'")
                return 1

        print("waiting for the probe (power it now if it is off)...")
        deadline = time.time() + 60
        while time.time() < deadline:
            link.probe("ping")
            if link.wait(lambda i: isinstance(i, Pong), 1.0):
                break
        else:
            print("probe never answered ping — UART build? D0/D1 crossover? (README)")
            return 1

        link.meter("stream on")
        for label, cmds in state_matrix(args.lora):
            link.meter(f"mark {label}")
            link.wait(lambda i: isinstance(i, Meter) and i.kind == "mark", 2.0)
            print(f"  {label}: {' ; '.join(cmds)}")
            for cmd in cmds:
                tag = markers.get(cmd.split()[0], "")
                link.probe(cmd)
                wait_s = 90.0 if cmd == "scan_networks" else 5.0
                r = link.wait(lambda i: isinstance(i, Error)
                              or (isinstance(i, Frame) and i.tag == tag), wait_s)
                if r is None:
                    errors.setdefault(label, []).append(f"no reply to {cmd}")
                elif isinstance(r, Error):
                    errors.setdefault(label, []).append(f"{cmd}: [ERR] {r.code}")
            link.probe("status")
            st = link.wait(lambda i: isinstance(i, Frame) and i.tag == markers["status"], 3.0)
            statuses[label] = (f"owner={st.get('owner')} lora={st.get('lora')}"
                               if st else "no status")
            if cmds != ["scan_networks"]:
                link.wait(lambda i: False, args.dwell)
        link.meter("mark end")
        end = link.wait(lambda i: isinstance(i, Meter) and i.kind == "mark", 2.0)
        link.meter("stream off")
        link.wait(lambda i: False, 0.3)
    finally:
        link.close()

    marks = [m for m in link.marks if m.kv.get("label") != "end"]
    if not link.windows or not marks:
        print("no windows or marks recorded — nothing to summarise")
        return 1
    end_us = end.int("t_us") if end else link.windows[-1].int("t1_us")
    figures = summarize(link.windows, marks, end_us, round(args.settle * 1e6),
                        link.probe_events, errors)
    write_run(args, info, link, figures, statuses)
    return 0 if all(f.valid for f in figures) else 2


def write_run(args, info: Meter, link: Link, figures, statuses) -> None:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = Path(args.out) if args.out else REPO / "power-runs" / stamp
    out.mkdir(parents=True, exist_ok=True)
    with open(out / "windows.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(WIN_INT_FIELDS)
        for rec in link.windows:
            w.writerow([rec.kv[k] for k in WIN_INT_FIELDS])
    with open(out / "summary.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["state", "t0_us", "t1_us", "windows", "mean_ua", "min_ua", "max_ua",
                    "vbus_min_mv", "status", "valid", "problems"])
        for s in figures:
            w.writerow([s.label, s.t0_us, s.t1_us, s.windows,
                        "" if s.mean_ua is None else round(s.mean_ua), s.min_ua, s.max_ua,
                        s.vbus_min_mv, statuses.get(s.label, ""), int(s.valid),
                        "; ".join(s.problems)])
    conditions = [
        f"utc={stamp}", f"rail={args.rail}", f"shunt_mohm={args.shunt_mohm}",
        f"window_ms={args.window_ms}", f"dwell_s={args.dwell}", f"settle_s={args.settle}",
        f"lora={args.lora}", f"meter: {' '.join(f'{k}={v}' for k, v in info.kv.items())}",
        f"note={args.note}",
    ]
    (out / "conditions.txt").write_text("\n".join(conditions) + "\n")

    print(f"\n{'state':<18} {'mean mA':>8} {'min':>7} {'peak':>7} {'Vmin':>6}  status / problems")
    for s in figures:
        ma = lambda v: "-" if v is None else f"{v / 1000:.1f}"  # noqa: E731
        vb = "-" if s.vbus_min_mv is None else f"{s.vbus_min_mv / 1000:.2f}"
        tail = "; ".join(s.problems) if s.problems else statuses.get(s.label, "")
        print(f"{s.label:<18} {ma(s.mean_ua):>8} {ma(s.min_ua):>7} {ma(s.max_ua):>7} {vb:>6}  "
              f"{'' if s.valid else 'INVALID: '}{tail}")
    print(f"\nwrote {out}")


def run_monitor(args) -> int:
    """Meter only, no probe needed: print 1 s windows until Ctrl-C."""
    link = Link(args.port)
    try:
        link.meter("stream off")
        link.wait(lambda i: False, 0.3)
        link.meter("info")
        info = link.wait(lambda i: isinstance(i, Meter) and i.kind == "info", 2.0)
        if info is None:
            print("meter did not answer #info — wrong port, or not the meter firmware?")
            return 1
        print(" ".join(f"{k}={v}" for k, v in info.kv.items()))
        if info.kv.get("ina") not in ("ina226", "ina228"):
            print("no INA226/INA228 yet — check VCC/GND/SDA(D4)/SCL(D5); waiting (Ctrl-C to quit)")
            while info is None or info.kv.get("ina") not in ("ina226", "ina228"):
                link.wait(lambda i: False, 2.0)
                link.meter("info")
                info = link.wait(lambda i: isinstance(i, Meter) and i.kind == "info", 2.0)
            print(" ".join(f"{k}={v}" for k, v in info.kv.items()))
        sag = RAILS[args.rail] if args.rail else 0
        for cmd in ("window 1000", f"shunt {round(args.shunt_mohm * 1000)}", f"sag {sag}"):
            if not meter_ok(link, cmd):
                print(f"meter refused '#{cmd}'")
                return 1
        link.meter("stream on")
        print(f"{'mean mA':>9} {'min':>8} {'peak':>8} {'Vbus':>6} {'Vmin':>6} {'n':>6}  flags")
        while True:
            item = link.wait(lambda i: isinstance(i, Meter) and i.kind in ("win", "probe"), 5.0)
            if item is None:
                print("no windows for 5 s — meter stalled?")
                continue
            if item.kind == "probe":
                print(f"  rail {item.kv.get('state')} at {item.kv.get('vbus_mv')} mV")
                continue
            print(format_window_row(item))
    except KeyboardInterrupt:
        pass
    finally:
        try:
            link.meter("stream off")
        finally:
            link.close()
    return 0


def format_window_row(w: Meter) -> str:
    ma = lambda k: f"{w.int(k) / 1000:.2f}"  # noqa: E731
    volts = lambda k: f"{w.int(k) / 1000:.2f}" if w.int(k) >= 0 else "-"  # noqa: E731
    flags = [f"{k}={w.int(k)}" for k in ("sag", "clip", "i2c_err", "lost") if w.int(k)]
    return (f"{ma('i_ua'):>9} {ma('i_min_ua'):>8} {ma('i_max_ua'):>8} {volts('vbus_mv'):>6} "
            f"{volts('vbus_min_mv'):>6} {w.int('n'):>6}  {' '.join(flags)}")


def selftest() -> int:
    from power_meter import parse_pwr
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    def win(t0, t1, i, lo, hi, sag=0, cuts=0, n=10):
        return (f"#PWR win t0_us={t0} t1_us={t1} n={n} i_ua={i} i_min_ua={lo} i_max_ua={hi} "
                f"vbus_mv=3300 vbus_min_mv=3290 sag={sag} clip=0 cuts={cuts} txdrop=0 "
                f"i2c_err=0 lost=0")

    stream = "\n".join([
        "ESP-ROM:esp32c5-eco2-20250121",
        "#PWR mark t_us=0 label=a",
        "[STATUS] owner=none lora=idle END",
        win(0, 1_000_000, 100_000, 90_000, 110_000),      # inside settle: excluded
        "[SCAN] BEGIN n=1",
        win(1_000_000, 2_000_000, 200_000, 150_000, 400_000),
        '[SCAN] 1 "#PWR win fake" -40',
        win(2_000_000, 4_000_000, 300_000, 250_000, 350_000),  # twice as long
        "[SCAN] END",
        "#PWR mark t_us=4000000 label=b",
        win(4_000_000, 5_000_000, 50_000, 40_000, 60_000, sag=1),
        "#PWR probe t_us=4500000 state=down vbus_mv=100",
        "#PWR win t0_us=oops",
        "",
    ]).encode()

    items = []
    d = Demux()
    for i in range(0, len(stream), 7):   # odd chunking
        items.extend(d.feed(stream[i:i + 7]))
    meters = [i for i in items if isinstance(i, Meter)]
    frames = [i for i in items if isinstance(i, Frame)]
    wins = [m for m in meters if m.kind == "win"]
    marks = [m for m in meters if m.kind == "mark"]
    probes = [m for m in meters if m.kind == "probe"]
    check("meter lines are split out of the OCP stream", len(wins) == 4 and len(marks) == 2)
    check("a malformed win record is dropped, not half-parsed", all("n" in w.kv for w in wins))
    check("OCP frames still parse around meter lines",
          [f.tag for f in frames] == ["[STATUS]", "[SCAN]"])
    check("'#PWR' inside a probe row stays probe data", len(frames[1].rows) == 1)
    check("quoted meter values decode", parse_pwr('#PWR err msg="too long"').kv["msg"] == "too long")
    check("a line that only starts with #PWR is not a record", parse_pwr("#PWRX win") is None)

    figs = summarize(wins, marks, 5_000_000, 1_000_000, probes, {"b": ["x: [ERR] busy"]})
    a, b = figs
    check("settle excludes the first window", a.windows == 2)
    check("mean is weighted by window duration",
          a.mean_ua is not None and abs(a.mean_ua - (200_000 + 2 * 300_000) / 3) < 1e-6)
    check("peak and floor come from the per-sample extremes", a.max_ua == 400_000 and a.min_ua == 150_000)
    check("a clean state is valid", a.valid)
    check("sag, power loss and command errors invalidate a state",
          not b.valid and "supply sagged" in b.problems and "probe lost power" in b.problems
          and "x: [ERR] busy" in b.problems)

    row = format_window_row(wins[1])
    check("monitor row shows mA and volts", "200.00" in row and "3.30" in row and row.endswith("  "))
    m = reply_markers()
    check("reply markers come from ocp.h", m.get("stop") == "[STOP]" and m.get("reboot") == "")
    for label, cmds in state_matrix("915000000 7 125 1"):
        for c in cmds:
            if c.split()[0] not in m:
                check(f"{label}: '{c}' is a verb in ocp.h", False)

    failed = [n for n, ok in checks if not ok]
    for n, ok in checks:
        print(f"  {'PASS' if ok else 'FAIL'}  {n}")
    print(f"power_sequence selftest: {len(checks) - len(failed)} passed, {len(failed)} failed")
    return 1 if failed else 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("port", nargs="?", help="the meter's /dev/serial/by-id/… path")
    ap.add_argument("--selftest", action="store_true", help="offline checks, no hardware")
    ap.add_argument("--monitor", action="store_true",
                    help="meter only: print 1 s windows until Ctrl-C (dry run, calibration)")
    ap.add_argument("--rail", choices=sorted(RAILS), help="where the shunt sits (required live)")
    ap.add_argument("--shunt-mohm", type=float,
                    help="calibrated shunt value in mΩ (required live; no default on purpose)")
    ap.add_argument("--window-ms", type=int, default=50)
    ap.add_argument("--dwell", type=float, default=20.0, help="seconds per state")
    ap.add_argument("--settle", type=float, default=2.0, help="seconds skipped after each transition")
    ap.add_argument("--lora", default="915000000 7 125 1",
                    help="lora_config args: freq_hz sf bw_khz cr_index")
    ap.add_argument("--out", help="output dir (default power-runs/<utc>/, git-ignored)")
    ap.add_argument("--note", default="", help="free text recorded in conditions.txt")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.monitor:
        if not args.port or args.shunt_mohm is None:
            ap.error("--monitor needs a port and --shunt-mohm")
        return run_monitor(args)
    if not args.port or not args.rail or args.shunt_mohm is None:
        ap.error("a live run needs a port, --rail and --shunt-mohm")
    if not 1.0 <= args.shunt_mohm <= 1000.0:
        ap.error("--shunt-mohm must be 1-1000 (the meter's accepted range)")
    return run_live(args)


if __name__ == "__main__":
    sys.exit(main())
