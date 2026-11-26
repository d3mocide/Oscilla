#!/usr/bin/env python3
"""deck_synth_caps.py — drive the deck's live tables past their caps on the bench.

Needs the probe on its synth bench build (./tools/build_firmware.sh
--synth-bench --probe-only, then flash), Grove to the deck as usual, and the
probe's USB port for the synth console. Nothing is transmitted: the probe
injects made-up `network` / `ble` events (02:5e:00/01:xx:xx:xx, "SYNTH-...")
into its normal event queue. Opens a wardrive session on the deck, runs
`synth`, samples counts every 15 s, closes the session.

If the deck has a GNSS fix, the synthetic rows ARE written to that drive's
SD files: delete that drive or never upload it.

Output: output/synth/<ts>/ (git-ignored), counts only.
  python3 tools/deck_synth_caps.py DECK_PORT PROBE_PORT WIFI_N BLE_N SECONDS [KEEP]
  python3 tools/deck_synth_caps.py /dev/ttyACM2 /dev/ttyACM0 600 300 600 8
"""
import json, sys, time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from deck_stability import Deck, Fault  # noqa: E402

deck_port, probe_port = sys.argv[1], sys.argv[2]
wifi_n, ble_n, seconds = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
keep = int(sys.argv[6]) if len(sys.argv) > 6 else 8

DUMP = ('link', 'link_errors', 'link_timeouts', 'link_resets', 'probe_owner', 'gnss',
        'scan_rows', 'bt_devices', 'scan_evicted', 'bt_evicted', 'tbl_scan', 'tbl_bt',
        'wardrive_open', 'wardrive_stream', 'wardrive_pending', 'wardrive_aps', 'wardrive_aps_nofix',
        'wardrive_bt', 'wardrive_bt_nofix', 'probe_heap', 'probe_heap_min', 'probe_uptime_ms')

out = ROOT / 'output' / 'synth' / time.strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True)
deck = Deck(deck_port, out, sensor=False)
probe = serial.Serial(probe_port, 115200, timeout=0.1)
probe_buf = b''
synth_last = {}


def probe_lines():
    global probe_buf
    probe_buf += probe.read(4096)
    while b'\n' in probe_buf:
        raw, probe_buf = probe_buf.split(b'\n', 1)
        line = raw.decode('utf-8', 'replace').strip()
        if line.startswith('S,'):
            parts = line.split(',')
            synth_last.clear()
            synth_last['state'] = parts[1]
            synth_last.update(p.split('=', 1) for p in parts[2:] if '=' in p)
            deck.record({'synth': line, 'elapsed_s': deck.elapsed()})


def sample(phase):
    d = deck.dump()
    m = deck.response('memory', 'memory')
    probe_lines()
    row = {'elapsed_s': deck.elapsed(), 'phase': phase,
           **{k: d.get(k) for k in DUMP},
           'deck_free': int(m['heap_free']), 'deck_largest': int(m['heap_largest']),
           'deck_min': int(m['heap_min']), 'deck_uptime_s': int(m['uptime_s']),
           'synth': dict(synth_last)}
    deck.record(row)
    deck.say(f"{phase}: scan {row['scan_rows']} ev {row['scan_evicted']} | bt {row['bt_devices']} "
             f"ev {row['bt_evicted']} | wd aps {row['wardrive_aps']}+{row['wardrive_aps_nofix']} "
             f"bt {row['wardrive_bt']}+{row['wardrive_bt_nofix']} pend {row['wardrive_pending']} | "
             f"deck {row['deck_free']}/{row['deck_largest']} | link {row['link']} "
             f"err {row['link_errors']}/{row['link_timeouts']}/{row['link_resets']} | "
             f"synth {synth_last.get('state')} w{synth_last.get('wifi')} b{synth_last.get('ble')} "
             f"rej {synth_last.get('rejected')}")
    return row


outcome = 'incomplete'
try:
    deck.say(f'output {out}')
    deck.collect(15)
    deck.send('\ndebug on', .6)
    deck.wait(lambda d: d['link'] == 'ready', 'link ready', 60)
    deck.armed = True
    deck.stop_phy()
    sample('before')
    deck.send('wardrive', 1.0)
    deck.wait(lambda d: d['wardrive_open'] == '1' and d['wardrive_stream'] == '1', 'wardrive streaming', 20)
    sample('wardrive_on')
    probe.write(f'synth {wifi_n} {ble_n} {seconds} {keep}\n'.encode())
    end = time.monotonic() + seconds + 90
    next_s = 0
    while time.monotonic() < end:
        if time.monotonic() >= next_s:
            sample('synth')
            next_s = time.monotonic() + 15
        deck.collect(.3)
        probe_lines()
    deck.send('wardrive', 1.0)                       # close the session
    deck.wait(lambda d: d['wardrive_open'] == '0', 'wardrive closed', 60)
    deck.collect(2)
    sample('after')
    outcome = 'completed'
except Fault as e:
    deck.say(f'FAULT {e}')
    outcome = 'fault: ' + str(e)
finally:
    try:
        probe.write(b'stop\n')
        time.sleep(.5)
        probe_lines()
    except Exception:
        pass
    deck.record({'outcome': outcome})
    deck.say('OUTCOME ' + outcome)
