#!/usr/bin/env python3
"""decode_xref.py — on-device OOK decode (O lines) vs rtl_433 and Flipper. Bench only.

Needs a probe on the `--cc1101-bench` build, an RTL-SDR with rtl_433, and a
Flipper as a known Princeton source (made-up key A5C3E1, te 400 us, four
bursts of 10 frames). LaCrosse TX141THBv2 packets are compared with rtl_433
reports; every other packet is listed as unexpected.

  python3 decode_xref.py --c5 <port> --flipper <port> --transmit [--seconds 330] [--lload on]
"""
import argparse, json, subprocess, sys, threading, time, serial
ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--c5", required=True)
ap.add_argument("--flipper", required=True)
ap.add_argument("--transmit", action="store_true")
ap.add_argument("--seconds", type=int, default=330)
ap.add_argument("--lload", choices=("on", "off"), default="on")
args = ap.parse_args()
if not args.transmit:
    sys.exit("refusing: the Flipper transmits test bursts; pass --transmit")
C5, FZ, SEC, LL = args.c5, args.flipper, args.seconds, args.lload
p=serial.Serial(C5,115200,timeout=0.2); time.sleep(5)   # opening the console may reset the C5
p.write(f"lload {LL}\nmode fifo 25000\n".encode()); time.sleep(2); p.write(b"raw off\ndrain 8\n")
P=[]; info=[]; ev=[]; stop=threading.Event()
def rd():
    while not stop.is_set():
        l=p.readline().decode(errors='replace').strip()
        if l.startswith('O,'): P.append((time.time(),l))
        elif l.startswith('I,partnum'): info.append(l)
        elif l.startswith('E,'): ev.append(l)
threading.Thread(target=rd,daemon=True).start()
rtl=subprocess.Popen(['rtl_433','-f','433.92M','-F','json','-M','time:unix:usec','-T',str(SEC)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
fz=serial.Serial(FZ,230400,timeout=0.2); fz.write(b"\x03\r\n"); time.sleep(1); fz.read(4096)
def bursts():
    for k in range(4):
        time.sleep(SEC/5)
        fz.write(b"subghz tx A5C3E1 433920000 400 10 0\r\n")
        buf=b""; e=time.time()+30
        while time.time()<e and not buf.rstrip().endswith(b">:"): buf+=fz.read(256)
threading.Thread(target=bursts,daemon=True).start()
R=[json.loads(l) for l in rtl.stdout if l.startswith('{')]
time.sleep(2); p.write(b"i\n"); time.sleep(1.5); stop.set()
lac=[l for _,l in P if 'lacrosse' in l]; pr=[l for _,l in P if 'princeton' in l]
rl=[j for j in R if 'LaCrosse' in j.get('model','')]
print(f"rtl_433 LaCrosse reports: {len(rl)}; on-device LaCrosse packets: {len(lac)}")
print("rtl_433 values:", sorted({(j['id'],j['temperature_C'],j['humidity'],j['battery_ok']) for j in rl}))
print("device values:", sorted({tuple(kv for kv in l.split(',') if kv.startswith(('id=','temp_c10=','hum=','batt_low='))) for l in lac}))
print(f"Princeton frames on device: {sum(int(l.split('frames=')[1].split(',')[0]) for l in pr)} (Flipper sent 40); codes {sorted({l.split('code=')[1] for l in pr})}")
other=[l for _,l in P if 'lacrosse' not in l and 'code=0xa5c3e1' not in l]
print("other packets:", other[:10])
print("events:", ev[:5])
if info: print("counters:", ",".join(kv for kv in info[-1].split(',') if kv.startswith(('fifo_overflows','fifo_dropped','fifo_late','ook_','lload_sweeps','lload_errors'))))
