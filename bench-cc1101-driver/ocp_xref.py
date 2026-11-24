#!/usr/bin/env python3
"""ocp_xref.py — OOK lane `kind=ook` events (OCP) vs rtl_433 and Flipper. Bench only.

Needs a probe on the normal firmware with OCP on its USB console
(`./tools/build_firmware.sh --bench`), an RTL-SDR with rtl_433, and a Flipper
as a known Princeton source (made-up key A5C3E1, te 400 us, four bursts of 10
frames). Unlike decode_xref.py this goes through the production path:
cc1101_capture -> ook_pulse -> ook_decode -> ook_events -> OCP event sender,
with repeat-merging, so each Flipper burst should be exactly ONE event.

--concurrent also runs the Wi-Fi sniffer and LoRa RX for the whole run, so
the comparison happens with all three lanes live (D-20).

  python3 ocp_xref.py --c5 <port> --flipper <port> --transmit [--seconds 330] [--concurrent]
"""
import argparse, json, subprocess, sys, threading, time

import serial

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--c5", required=True)
ap.add_argument("--flipper", required=True)
ap.add_argument("--transmit", action="store_true")
ap.add_argument("--seconds", type=int, default=330)
ap.add_argument("--concurrent", action="store_true")
args = ap.parse_args()
if not args.transmit:
    sys.exit("refusing: the Flipper transmits test bursts; pass --transmit")
SEC = args.seconds
BURSTS = 4


def kv(line):
    return dict(t.split("=", 1) for t in line.split() if "=" in t)


p = serial.Serial(args.c5, 115200, timeout=0.2)
time.sleep(5)                       # opening the console may reset the C5
lines, stop = [], threading.Event()


def rd():
    while not stop.is_set():
        l = p.readline().decode(errors="replace").strip()
        if l:
            lines.append((time.time(), l))


threading.Thread(target=rd, daemon=True).start()


def cmd(c, wait=1.0):
    n = len(lines)
    p.write((c + "\n").encode())
    time.sleep(wait)
    return [l for _, l in lines[n:]]


cmd("stop", 1.0)
if args.concurrent:
    cmd("lora_config 915000000 7 125 1")
    cmd("lora_listen", 1.0)
    cmd("start_sniffer", 1.5)
print("listen:", [l for l in cmd("ook_listen 433920", 1.0) if l.startswith("[OOK]")])
t0 = time.time()

rtl = subprocess.Popen(["rtl_433", "-f", "433.92M", "-F", "json", "-M", "time:unix:usec", "-T", str(SEC)],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
fz = serial.Serial(args.flipper, 230400, timeout=0.2)
fz.write(b"\x03\r\n"); time.sleep(1); fz.read(4096)
sent = []


def bursts():
    for _ in range(BURSTS):
        time.sleep(SEC / (BURSTS + 1))
        sent.append(time.time())
        fz.write(b"subghz tx A5C3E1 433920000 400 10 0\r\n")
        buf, e = b"", time.time() + 30
        while time.time() < e and not buf.rstrip().endswith(b">:"):
            buf += fz.read(256)


threading.Thread(target=bursts, daemon=True).start()
R = [json.loads(l) for l in rtl.stdout if l.startswith("{")]
time.sleep(2)
status = [l for l in cmd("ook_status", 1.0) if l.startswith("[OOK]")]
st = [l for l in cmd("status", 1.0) if l.startswith("[STATUS]")]
cmd("stop", 1.0)
stop.set()

ev = [(t, kv(l)) for t, l in lines if l.startswith("[EVT]") and "kind=ook" in l and t >= t0]
ook = [(t, e) for t, e in ev if e.get("kind") == "ook"]
unk = [e for _, e in ev if e.get("kind") == "ook_unknown"]
lac = [e for _, e in ook if e.get("decoder") == "lacrosse_tx141thbv2"]
pr = [(t, e) for t, e in ook if e.get("decoder") == "princeton"]
rl = [j for j in R if "LaCrosse" in j.get("model", "")]

print(f"rtl_433 LaCrosse reports: {len(rl)}; OCP LaCrosse events: {len(lac)}")
print("rtl_433 values:", sorted({(j["id"], j["temperature_C"], j["humidity"], j["battery_ok"]) for j in rl}))
print("OCP values:    ", sorted({(int(e["id"]), int(e["temp_c10"]) / 10, int(e["hum"]), 1 - int(e["batt_low"])) for e in lac}))
print(f"Princeton events: {len(pr)} for {len(sent)} bursts; hex {sorted({e.get('hex') for _, e in pr})}; "
      f"frames per event {[e.get('frames') for _, e in pr]}")
print("burst->event latency s:", [round(min((t - s for t, _ in pr if t >= s), default=-1), 2) for s in sent])
other = [e for _, e in ook if e.get("decoder") not in ("lacrosse_tx141thbv2",) and e.get("hex") != "a5c3e1"]
print("other packets:", other[:10])
print(f"ook_unknown summaries: {len(unk)}; first: {unk[:3]}")
print("ook_status:", status[-1] if status else None)
print("status:", st[-1] if st else None)
