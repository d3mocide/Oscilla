#!/usr/bin/env python3
"""soak.py — long all-lanes soak of the probe over OCP (USB bench build). Bench only.

Runs the three lanes at once for --hours: Wi-Fi sniffer (PHY lane), LoRa RX
and OOK RX. Every --cycle-min minutes the PHY lane is rotated through a full
scan_networks and a 60 s BLE scan before the sniffer resumes, so the heap
sees realistic allocate/free churn rather than one steady state.

Every --sample-s seconds one CSV row records counters only: heap, low-water
mark, largest block, PSRAM, lane states, OOK and LoRa counters, event counts
per kind and command latency. No addresses, SSIDs or payloads are written.

Optional references:
  --rtl          run rtl_433 alongside and count LaCrosse reports (needs an RTL-SDR)
  --flipper P --transmit
                 a Flipper sends the made-up Princeton key A5C3E1 every
                 --burst-min minutes; each burst must produce one kind=ook event

Output goes to output/soak/<timestamp>/ (git-ignored): soak.csv, events.txt
(anomalies only), summary.txt (rewritten every row), rtl433.json.

  python3 soak.py --c5 <port> --hours 10 --rtl [--flipper <port> --transmit]
  python3 soak.py --summarize output/soak/<timestamp>
"""
import argparse, csv, json, os, queue, re, subprocess, sys, threading, time
from collections import Counter
from pathlib import Path

KEY = "A5C3E1"
ROOT = Path(__file__).resolve().parent.parent


def kv(line):
    return dict(t.split("=", 1) for t in line.split() if "=" in t)


class Probe:
    def __init__(self, port):
        import serial
        self.ser = serial.Serial(port, 115200, timeout=0.1)
        self.lines = queue.Queue()
        self.kinds = Counter()
        self.ook_events = []          # (time, decoder, hex)
        self.hellos = 0
        self.errs = Counter()
        self.stop = False
        threading.Thread(target=self._rd, daemon=True).start()

    def _rd(self):
        buf = b""
        while not self.stop:
            try:
                buf += self.ser.read(4096)
            except Exception as e:      # USB dropped: surface it, keep trying
                self.lines.put(("!", f"serial error: {e}"))
                time.sleep(1)
                continue
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                l = raw.decode(errors="replace").strip()
                if not l:
                    continue
                now = time.time()
                if l.startswith("[EVT]"):
                    k = kv(l).get("kind", "?")
                    self.kinds[k] += 1
                    if k == "ook":
                        e = kv(l)
                        self.ook_events.append((now, e.get("decoder"), e.get("hex")))
                    continue
                if l.startswith("[HELLO]"):
                    self.hellos += 1
                if l.startswith("[ERR]"):
                    self.errs[kv(l).get("code", "?")] += 1
                self.lines.put((now, l))

    def drain(self):
        while not self.lines.empty():
            self.lines.get_nowait()

    def cmd(self, c, marker, timeout=3.0):
        """Send c; return (first line starting with marker or [ERR], latency_ms)."""
        self.drain()
        t0 = time.time()
        self.ser.write((c + "\n").encode())
        while time.time() - t0 < timeout:
            try:
                _, l = self.lines.get(timeout=0.2)
            except queue.Empty:
                continue
            if l.startswith(marker) or l.startswith("[ERR]"):
                return l, int((time.time() - t0) * 1000)
        return None, None

    def block(self, c, marker, timeout):
        """Send c; wait for the block frame's END. Returns (rows, latency_ms)."""
        self.drain()
        t0 = time.time()
        self.ser.write((c + "\n").encode())
        rows = 0
        while time.time() - t0 < timeout:
            try:
                _, l = self.lines.get(timeout=0.2)
            except queue.Empty:
                continue
            if l.startswith("[ERR]"):
                return -1, None
            if l.startswith(marker + " END") or (l.startswith(marker) and l.endswith(" END") and "BEGIN" not in l):
                return rows, int((time.time() - t0) * 1000)
            if l.startswith(marker) and "BEGIN" not in l:
                rows += 1
        return None, None


def lanes_up(p, log):
    p.cmd("stop", "[STOP]", 4)
    p.cmd("lora_config 915000000 7 125 1", "[CFG]")
    a, _ = p.cmd("lora_listen", "[LORA]")
    b, _ = p.cmd("ook_listen 433920", "[OOK]")
    c, _ = p.cmd("start_sniffer", "[SNIFF]", 4)
    ok = all(x and not x.startswith("[ERR]") for x in (a, b, c))
    if not ok:
        log(f"lanes_up failed: lora={a} ook={b} sniff={c}")
    return ok


FIELDS = ["t", "elapsed_s", "uptime_ms", "rst", "heap", "heap_min", "heap_largest", "psram_free",
          "owner", "lora", "ook", "status_ms",
          "ook_trains", "ook_packets", "ook_unknown", "ook_overflows", "ook_dropped", "ook_errors",
          "ook_reinit", "ook_hw_fault", "ook_ocp_drop",
          "lora_rx", "lora_crc_err", "lora_header_err", "lora_irq_drop", "lora_radio_drop",
          "lora_ocp_drop", "lora_hw_fault",
          "ev_total", "ev_sniff", "ev_ook", "ev_ook_unknown", "ev_ble", "ev_other",
          "hellos", "errs", "phase"]


def summarize(d):
    d = Path(d)
    rows = list(csv.DictReader(open(d / "soak.csv")))
    if not rows:
        return "no rows"
    f = lambda k: [int(r[k]) for r in rows if r[k] not in ("", None)]
    heap, mins, larg = f("heap"), f("heap_min"), f("heap_largest")
    steady = [(float(r["elapsed_s"]), int(r["heap"])) for r in rows if r["phase"] == "steady" and r["heap"]]
    slope = 0.0
    if len(steady) > 2:
        n = len(steady); mx = sum(x for x, _ in steady) / n; my = sum(y for _, y in steady) / n
        den = sum((x - mx) ** 2 for x, _ in steady)
        slope = sum((x - mx) * (y - my) for x, y in steady) / den * 3600 if den else 0.0
    last = rows[-1]
    lat = f("status_ms")
    out = [
        f"rows {len(rows)}  elapsed {float(last['elapsed_s'])/3600:.2f} h  uptime {int(last['uptime_ms'] or 0)/3.6e6:.2f} h  rst={last['rst']}",
        f"heap   first {heap[0]}  last {heap[-1]}  min-sampled {min(heap)}  steady slope {slope:+.0f} B/h",
        f"heap_min (since boot) last {mins[-1]}   largest block min {min(larg)}",
        f"status latency ms  median {sorted(lat)[len(lat)//2] if lat else '-'}  max {max(lat) if lat else '-'}",
        f"ook  trains {last['ook_trains']} packets {last['ook_packets']} unknown {last['ook_unknown']} "
        f"overflows {last['ook_overflows']} dropped {last['ook_dropped']} errors {last['ook_errors']} "
        f"reinit {last['ook_reinit']} hw_fault {last['ook_hw_fault']} ocp_drop {last['ook_ocp_drop']}",
        f"lora rx {last['lora_rx']} crc_err {last['lora_crc_err']} irq_drop {last['lora_irq_drop']} "
        f"radio_drop {last['lora_radio_drop']} ocp_drop {last['lora_ocp_drop']} hw_fault {last['lora_hw_fault']}",
        f"events total {last['ev_total']} (sniff {last['ev_sniff']}, ook {last['ev_ook']}, "
        f"ook_unknown {last['ev_ook_unknown']}, ble {last['ev_ble']})  hellos {last['hellos']}  errs {last['errs']}",
    ]
    ev = d / "events.txt"
    if ev.exists():
        lines = ev.read_text().splitlines()
        an = [x for x in lines if "ANOMALY" in x]
        out.append(f"event log {len(lines)} lines, ANOMALIES {len(an)}" + ("" if not an else f"; last: {an[-1][:120]}"))
    rtl = d / "rtl433.json"
    if rtl.exists():
        n = sum(1 for l in open(rtl) if "LaCrosse" in l)
        out.append(f"rtl_433 LaCrosse reports {n} (2 per reading; compare with OCP LaCrosse events in events.txt)")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--c5")
    ap.add_argument("--hours", type=float, default=10)
    ap.add_argument("--sample-s", type=int, default=30)
    ap.add_argument("--cycle-min", type=int, default=15)
    ap.add_argument("--burst-min", type=int, default=30)
    ap.add_argument("--rtl", action="store_true")
    ap.add_argument("--flipper")
    ap.add_argument("--transmit", action="store_true")
    ap.add_argument("--out")
    ap.add_argument("--summarize", metavar="DIR")
    a = ap.parse_args()
    if a.summarize:
        print(summarize(a.summarize)); return
    if not a.c5:
        sys.exit("--c5 required")
    if a.flipper and not a.transmit:
        sys.exit("refusing: the Flipper transmits test bursts; pass --transmit")

    out = Path(a.out or ROOT / "output" / "soak" / time.strftime("%Y%m%d-%H%M%S"))
    out.mkdir(parents=True, exist_ok=True)
    evf = open(out / "events.txt", "a", buffering=1)

    def log(msg):
        evf.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {msg}\n")

    p = Probe(a.c5)
    time.sleep(6)                       # opening the console may reset the C5
    rtl = None
    if a.rtl:
        rtl = subprocess.Popen(["rtl_433", "-f", "433.92M", "-F", f"json:{out / 'rtl433.json'}",
                                "-M", "time:unix:usec"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    fz = None
    if a.flipper:
        import serial
        fz = serial.Serial(a.flipper, 230400, timeout=0.2)
        fz.write(b"\x03\r\n"); time.sleep(1); fz.read(4096)

    lanes_up(p, log)
    w = csv.DictWriter(open(out / "soak.csv", "w", newline="", buffering=1), fieldnames=FIELDS)
    w.writeheader()
    t_start = time.time(); t_end = t_start + a.hours * 3600
    next_cycle = t_start + a.cycle_min * 60
    next_burst = t_start + 120
    last_uptime = None; hellos_seen = p.hellos; lac_logged = 0
    bursts = hits = 0

    while time.time() < t_end:
        phase = "steady"
        now = time.time()
        if now >= next_cycle:
            phase = "cycle"
            next_cycle = now + a.cycle_min * 60
            p.cmd("stop phy", "[STOP]", 4)
            rows, ms = p.block("scan_networks", "[SCAN]", 40)
            log(f"cycle: scan_networks rows={rows} ms={ms}" if rows is not None and rows >= 0
                else f"ANOMALY scan_networks reply missing or error (rows={rows})")
            b0 = p.kinds["ble"]
            r, _ = p.cmd("start_ble_scan", "[BLE]", 4)
            time.sleep(60)
            p.cmd("stop phy", "[STOP]", 4)
            nble = p.kinds["ble"] - b0
            log(f"cycle: ble events {nble} in 60 s" if nble else f"ANOMALY ble scan produced no events ({r})")
            r, _ = p.cmd("start_sniffer", "[SNIFF]", 4)
            if not r or r.startswith("[ERR]"):
                log(f"ANOMALY sniffer restart failed: {r}")
        if fz and now >= next_burst:
            next_burst = now + a.burst_min * 60
            bursts += 1
            t_b = time.time()
            fz.write(f"subghz tx {KEY} 433920000 400 10 0\r\n".encode())
            time.sleep(4)
            hit = [e for e in p.ook_events if e[0] >= t_b and e[1] == "princeton" and (e[2] or "").lower() == KEY.lower()]
            if hit:
                hits += 1
                log(f"burst {bursts}: event after {hit[0][0] - t_b:.2f} s")
            else:
                log(f"ANOMALY burst {bursts}: no princeton {KEY} event within 4 s")

        st, st_ms = p.cmd("status", "[STATUS]")
        oo, _ = p.cmd("ook_status", "[OOK]")
        lo, _ = p.cmd("lora_status", "[LORA]")
        s, o, l = kv(st or ""), kv(oo or ""), kv(lo or "")
        if not st:
            log("ANOMALY no [STATUS] reply")
        up = int(s.get("uptime_ms", 0) or 0)
        if last_uptime is not None and st and up < last_uptime:
            log(f"ANOMALY probe rebooted (uptime {last_uptime} -> {up}, rst={s.get('rst')}); restarting lanes")
            lanes_up(p, log)
        if p.hellos != hellos_seen:
            log(f"ANOMALY unsolicited [HELLO] x{p.hellos - hellos_seen}")
            hellos_seen = p.hellos
        if st:
            last_uptime = up
            if phase == "steady" and (s.get("ook") != "rx" or s.get("lora") != "rx" or s.get("owner") != "wifi"):
                log(f"ANOMALY lane down: owner={s.get('owner')} lora={s.get('lora')} ook={s.get('ook')}; restarting")
                lanes_up(p, log)
        lac = sum(1 for e in p.ook_events if e[1] == "lacrosse_tx141thbv2")
        if lac != lac_logged:
            log(f"lacrosse events total {lac}")
            lac_logged = lac
        k = p.kinds
        w.writerow({
            "t": time.strftime("%Y-%m-%d %H:%M:%S"), "elapsed_s": int(time.time() - t_start),
            "uptime_ms": s.get("uptime_ms", ""), "rst": s.get("rst", ""),
            "heap": s.get("heap", ""), "heap_min": s.get("heap_min", ""),
            "heap_largest": s.get("heap_largest", ""), "psram_free": s.get("psram_free", ""),
            "owner": s.get("owner", ""), "lora": s.get("lora", ""), "ook": s.get("ook", ""),
            "status_ms": st_ms or "",
            "ook_trains": o.get("trains", ""), "ook_packets": o.get("packets", ""),
            "ook_unknown": o.get("unknown", ""), "ook_overflows": o.get("overflows", ""),
            "ook_dropped": o.get("dropped", ""), "ook_errors": o.get("errors", ""),
            "ook_reinit": o.get("reinit", ""), "ook_hw_fault": o.get("hw_fault", ""),
            "ook_ocp_drop": o.get("ocp_drop", ""),
            "lora_rx": l.get("rx", ""), "lora_crc_err": l.get("crc_err", ""),
            "lora_header_err": l.get("header_err", ""), "lora_irq_drop": l.get("irq_drop", ""),
            "lora_radio_drop": l.get("radio_drop", ""), "lora_ocp_drop": l.get("ocp_drop", ""),
            "lora_hw_fault": l.get("hw_fault", ""),
            "ev_total": sum(k.values()), "ev_sniff": k["sniff"], "ev_ook": k["ook"],
            "ev_ook_unknown": k["ook_unknown"], "ev_ble": k["ble"],
            "ev_other": sum(v for x, v in k.items() if x not in ("sniff", "ook", "ook_unknown", "ble")),
            "hellos": p.hellos, "errs": sum(p.errs.values()), "phase": phase,
        })
        (out / "summary.txt").write_text(summarize(out) + f"\nflipper bursts {bursts} hits {hits}\n")
        time.sleep(max(0, a.sample_s - (time.time() - now)))

    p.cmd("stop", "[STOP]", 4)
    p.stop = True
    if rtl:
        rtl.terminate()
    (out / "summary.txt").write_text(summarize(out) + f"\nflipper bursts {bursts} hits {hits}\n")
    print(out / "summary.txt")
    print((out / "summary.txt").read_text())


if __name__ == "__main__":
    main()
