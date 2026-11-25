#!/usr/bin/env python3
"""deck_stability.py — combined deck + probe stability run over the deck's debug console.

Bench only. The probe runs its normal UART build and the deck drives it over
Grove exactly as in the field; this script only types debug-console commands
into the deck's USB port and reads counts back. Nothing here transmits: every
lane started is a receive lane (Wi-Fi, BLE, LoRa RX, OOK RX).

Stages (minutes are defaults):
  all_lanes  45  Wi-Fi live + LoRa RX + OOK RX together; the deck moves through
                 cards (OOK, GNSS, LoRa, Wi-Fi, System, sessions) every minute
  switching  30  LoRa + OOK stay on while the PHY lane rotates through 12 modes
  use_idle   40  4 cycles of 5 min active use / 5 min with every lane stopped

Each sample (every 30 s) records counts only: deck and probe heap, loop stack
margin, link counters, lane flags, OOK/LoRa packet counts, SD and display
counters. No addresses, SSIDs or payloads. Output: output/deck-stability/<ts>/
(git-ignored): samples.jsonl and progress.log.

The run stops with outcome=fault on: a deck or probe reset, a link error /
timeout / reset, SD write or UART errors, LoRa hardware fault, loop stack under
2 KiB, a lane that should be running found stopped, or (with the default
--sensor) no new OOK packet for 5 minutes while OOK should be listening.
Touch output/deck-stability/STOP to end early; the lanes are stopped either way.

  python3 tools/deck_stability.py --deck /dev/ttyACM2
  python3 tools/deck_stability.py --deck /dev/ttyACM2 --all-lanes 10 --switching 0 --cycles 1   # smoke
"""
import argparse
import fcntl
import json
import re
import statistics
import termios
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[1]
RESET_RE = re.compile(r'^(Guru Meditation|Backtrace:|Rebooting|ESP-ROM|rst:|Brownout|abort\(|assert failed|deck memory tag=boot)')

# name, card, start command, owner, dump flag, bounded (snapshot that ends by itself)
MODES = [
    ('wifi_live', 'wifi', 'wifiscan', 'wifi', 'wifi_live', False),
    ('ble_snapshot', 'beacons', 'beacons', None, 'bt_scanning', True),
    ('sniffer', 'contacts', 'sniff', 'wifi', 'sniffer_active', False),
    ('packet_monitor', 'spectrum', 'channel 6', 'wifi', 'spectrum_active', False),
    ('deauth', 'deauth', 'deauth', 'wifi', 'deauth_active', False),
    ('ble_live', 'beacons', 'blescan', 'ble', 'bt_live', False),
    ('wifi_snapshot', 'wifi', 'scan', None, 'wifi_scanning', True),
    ('tracker_watch', 'beacons', 'airtag', 'ble', 'bt_tracker', False),
    ('sniffer', 'contacts', 'sniff', 'wifi', 'sniffer_active', False),
    ('packet_monitor', 'spectrum', 'channel 6', 'wifi', 'spectrum_active', False),
    ('antisurveillance', 'antisurv', 'antisurv', 'ble', 'anti_active', False),
    ('wifi_live', 'wifi', 'wifiscan', 'wifi', 'wifi_live', False),
]
PHY_FLAGS = ('wifi_live', 'wifi_scanning', 'bt_live', 'bt_scanning', 'bt_tracker', 'sniffer_active',
             'spectrum_active', 'deauth_active', 'anti_active')
TOUR = ['ook', 'key .', 'key ;', 'gnss', 'lora', 'wifi', 'info']


class Fault(Exception):
    pass


class Stop(Exception):
    pass


def kv(line):
    return dict(re.findall(r'(\w+)=([^ ]+)', line))


class Deck:
    def __init__(self, port, out, sensor):
        self.s = serial.Serial(port=None, baudrate=115200, timeout=.1, exclusive=True)
        self.s.dtr = False
        self.s.rts = False          # opening must never reset the deck
        self.s.port = port
        self.s.open()
        fcntl.ioctl(self.s.fileno(), termios.TIOCEXCL)
        attrs = termios.tcgetattr(self.s.fileno())
        attrs[2] &= ~termios.HUPCL
        termios.tcsetattr(self.s.fileno(), termios.TCSANOW, attrs)
        self.out = out
        self.jsonl = (out / 'samples.jsonl').open('w')
        self.log = (out / 'progress.log').open('w')
        self.stop_file = out.parent / 'STOP'
        self.sensor = sensor
        self.pending = b''
        self.latest = {}
        self.events = {'probe_resets': 0, 'link_transitions': 0, 'retries': 0}
        self.origin = time.monotonic()
        self.samples = []
        self.expected = {}
        self.baseline = None
        self.ook_seen = (0, time.monotonic())
        self.armed = False          # boot output before preflight is expected, not a fault
        self.boots = 0

    # ---- serial ----------------------------------------------------------
    def say(self, text):
        line = f'{time.strftime("%H:%M:%S")} {text}'
        print(line, flush=True)
        self.log.write(line + '\n')
        self.log.flush()

    def record(self, obj):
        self.jsonl.write(json.dumps(obj) + '\n')
        self.jsonl.flush()

    def collect(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if self.stop_file.exists():
                raise Stop()
            self.pending += self.s.read(max(1, self.s.in_waiting))
            if len(self.pending) > 65536:
                raise Fault('serial line exceeded bound')
            while b'\n' in self.pending:
                raw, self.pending = self.pending.split(b'\n', 1)
                line = raw.decode('utf-8', 'replace').strip()
                for prefix, name in (('deck dump ', 'dump'), ('deck memory ', 'memory'),
                                     ('deck display_status ', 'storage'), ('deck viewport ', 'viewport')):
                    if line.startswith(prefix):
                        self.latest[name] = kv(line)
                        break
                if RESET_RE.match(line):
                    self.record({'diagnostic': line, 'elapsed_s': self.elapsed(), 'armed': self.armed})
                    if self.armed:
                        raise Fault('deck reset diagnostic: ' + line[:80])
                    self.boots += 1
                if line.startswith('deck probe-reset '):
                    self.events['probe_resets'] += 1
                if line.startswith('deck state='):
                    self.events['link_transitions'] += 1

    def send(self, command, seconds=.35):
        self.s.write((command + '\n').encode())
        self.s.flush()
        self.collect(seconds)

    def response(self, command, name):
        for _ in range(2):
            self.latest.pop(name, None)
            self.s.write((command + '\n').encode())
            self.s.flush()
            deadline = time.monotonic() + 5
            while name not in self.latest and time.monotonic() < deadline:
                self.collect(.1)
            if name in self.latest:
                return self.latest[name]
            self.events['retries'] += 1
        raise Fault('no telemetry reply: ' + name)

    def dump(self):
        return self.response('dump', 'dump')

    def elapsed(self):
        return round(time.monotonic() - self.origin, 2)

    def wait(self, predicate, label, seconds=15):
        deadline = time.monotonic() + seconds
        while True:
            d = self.dump()
            if predicate(d):
                return d
            if time.monotonic() > deadline:
                raise Fault('timed out: ' + label)
            self.collect(.3)

    # ---- samples ---------------------------------------------------------
    def take(self, phase, check=True):
        for name in ('dump', 'memory', 'storage', 'viewport'):
            self.latest.pop(name, None)
        self.response('dump', 'dump')
        self.response('memory', 'memory')
        self.response('display_status', 'storage')
        if 'viewport' not in self.latest:
            self.collect(.5)
        d, m, h = self.latest['dump'], self.latest['memory'], self.latest['storage']
        v = self.latest.get('viewport', {})
        row = {'elapsed_s': self.elapsed(), 'phase': phase,
               'deck_free': int(m['heap_free']), 'deck_min': int(m['heap_min']),
               'deck_largest': int(m['heap_largest']), 'deck_uptime_s': int(m['uptime_s']),
               'stack_free_min': int(m.get('stack_free_min', 0)),
               'probe_free': int(d['probe_heap']), 'probe_min': int(d['probe_heap_min']),
               'probe_largest': int(d['probe_heap_largest']), 'probe_uptime_ms': int(d['probe_uptime_ms']),
               'probe_rst': d['probe_rst'], 'probe_owner': d['probe_owner'], 'probe_ook': d['probe_ook'],
               'link_ready': int(d['link'] == 'ready'),
               'uart_errors': int(h['uart_errors']), 'write_errors': int(h['write_errors']),
               'packet_drops': int(h['packet_drops']), 'health_drops': int(h['health_drops']),
               'lora_open': int(h['lora_open']), 'sd_ready': int(h['sd_ready']),
               'viewport_frames': int(v.get('frames', 0)), 'max_cycle_us': int(v.get('max_cycle_us', 0)),
               **self.events}
        for key in ('link_errors', 'link_timeouts', 'link_resets', 'lora_active', 'lora_rx', 'lora_hw_fault',
                    'ook_active', 'ook_devices', 'ook_packets', 'ook_unknown', 'scan_rows', 'bt_devices',
                    'sniff_pkts', 'probe_status_seq', *PHY_FLAGS):
            row[key] = int(d[key])
        self.record(row)
        if self.samples and row['deck_uptime_s'] < self.samples[-1]['deck_uptime_s']:
            raise Fault('deck uptime went backwards (deck reset)')
        if self.samples and row['probe_uptime_ms'] + 5000 < self.samples[-1]['probe_uptime_ms']:
            raise Fault('probe uptime went backwards (probe reset)')
        self.samples.append(row)
        if check:
            self.check(row)
        return row

    def check(self, row):
        bad = [k for k in ('write_errors', 'uart_errors', 'lora_hw_fault', 'packet_drops', 'health_drops',
                           'probe_resets') if row[k]]
        if not row['link_ready']:
            bad.append('link not ready')
        if bad:
            raise Fault('device fault: ' + ', '.join(bad))
        if row['stack_free_min'] < 2048:
            raise Fault('deck loop stack margin below 2 KiB')
        if self.baseline is not None:
            changed = [k for k, v in self.baseline.items() if row[k] != v]
            if changed:
                raise Fault('link counter changed: ' + ', '.join(changed))
        wrong = {k: row[k] for k, v in self.expected.items() if row[k] != v}
        if wrong:
            raise Fault(f'lane state changed: expected {self.expected}, got {wrong}')
        if self.expected.get('ook_active') == 1 and self.sensor:
            n, since = self.ook_seen
            if row['ook_packets'] != n:
                self.ook_seen = (row['ook_packets'], time.monotonic())
            elif time.monotonic() - since > 300:
                raise Fault('no new OOK packet for 5 minutes (sensor stalled?)')

    def brief(self, row):
        return (f"deck {row['deck_free']}/{row['deck_largest']} stack {row['stack_free_min']} | "
                f"probe {row['probe_free']}/{row['probe_largest']} | ook {row['ook_packets']} "
                f"lora {row['lora_rx']} | frames {row['viewport_frames']}")

    # ---- lanes -----------------------------------------------------------
    def stop_phy(self):
        before = int(self.dump()['probe_status_seq'])
        self.send('stop phy', .4)
        return self.wait(lambda d: int(d['probe_status_seq']) > before and d['probe_owner'] == 'none'
                         and all(d[k] == '0' for k in PHY_FLAGS), 'PHY stopped')

    def start_receivers(self):
        d = self.dump()
        if d['lora_active'] == '0':
            if d['lora_cfg'] == 'none':       # a freshly booted deck has not sent a LoRa config yet
                self.send('lora config', 1.0)
                self.wait(lambda d: d['lora_cfg'] != 'none', 'LoRa config accepted')
            self.send('lora', 1.5)
            self.wait(lambda d: d['lora_active'] == '1', 'LoRa RX started')
        if d['ook_active'] == '0':
            self.send('ook', 1.0)
            self.wait(lambda d: d['ook_active'] == '1' and d['probe_ook'] == 'rx', 'OOK RX started')
        self.ook_seen = (int(self.dump()['ook_packets']), time.monotonic())

    def stop_receivers(self):
        self.send('stop lora', .6)
        self.send('stop ook', .6)
        return self.wait(lambda d: d['lora_active'] == '0' and d['ook_active'] == '0', 'LoRa and OOK stopped')

    def start_mode(self, mode):
        name, card, command, owner, flag, bounded = mode
        self.expected = {'lora_active': 1, 'ook_active': 1}
        self.stop_phy()
        self.send('card ' + card, .3)
        before = int(self.dump()['probe_status_seq'])
        self.send(command, .4)
        if bounded:
            self.wait(lambda d: d[flag] == '1', name + ' started', 8)
        else:
            self.wait(lambda d: d[flag] == '1' and d['probe_owner'] == owner
                      and int(d['probe_status_seq']) > before, name + ' ownership')
            self.expected.update({flag: 1, 'probe_owner': owner})

    def tour(self, mode=None):
        """One pass through the cards a person would look at, with keys.

        Leaving a Wi-Fi/BLE card stops the PHY lane by design (the visible
        card owns that radio; LoRa and OOK keep running), so the tour drops
        the PHY expectations, looks around, then goes back and restarts the
        mode the way a person would."""
        for k in (*PHY_FLAGS, 'probe_owner'):
            self.expected.pop(k, None)
        for step in TOUR:
            self.send(step if step.startswith('key') else 'card ' + step, .8)
        self.send('key i', .3)
        self.send('key back', .3)
        if mode:
            self.start_mode(mode)

    def run_until(self, end, phase, mode=None, tour_every=60):
        next_sample = time.monotonic()
        next_tour = time.monotonic() + 20 if tour_every else float('inf')
        next_refresh = time.monotonic() + 40
        last_progress = 0.0
        while time.monotonic() < end:
            now = time.monotonic()
            if mode and mode[5] and now >= next_refresh:      # keep snapshot modes cycling
                if self.dump()[mode[4]] == '0':
                    self.send(mode[2], .3)
                next_refresh = now + 45
            if now >= next_tour:
                self.tour(mode)
                next_tour = time.monotonic() + tour_every
            if now >= next_sample:
                row = self.take(phase)
                next_sample = now + 30
                if now - last_progress >= 120:
                    self.say(f'{phase}: {self.brief(row)}')
                    last_progress = now
            self.collect(.15)


def summarize(samples):
    def fit(rows, key):
        if len(rows) < 3:
            return None
        xs = [r['elapsed_s'] / 3600 for r in rows]
        ys = [r[key] for r in rows]
        xm, ym = statistics.mean(xs), statistics.mean(ys)
        den = sum((x - xm) ** 2 for x in xs)
        slope = sum((x - xm) * (y - ym) for x, y in zip(xs, ys)) / den if den else 0
        return {'first': ys[0], 'last': ys[-1], 'min': min(ys), 'max': max(ys), 'slope_per_h': round(slope)}
    warm = [r for r in samples if r['elapsed_s'] >= (samples[0]['elapsed_s'] + 600 if samples else 0)]
    return {k: fit(warm, k) for k in ('deck_free', 'deck_largest', 'probe_free', 'probe_largest')} | {
        'samples': len(samples),
        'min_stack': min((r['stack_free_min'] for r in samples), default=None),
        'ook_packets': samples[-1]['ook_packets'] if samples else 0,
        'max_cycle_us': max((r['max_cycle_us'] for r in samples), default=0),
        'note': 'heap slopes exclude the first 10 min; trends are observations, not verdicts'}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--deck', required=True, help='deck USB port (debug console)')
    ap.add_argument('--all-lanes', type=float, default=45, help='minutes')
    ap.add_argument('--switching', type=float, default=30, help='minutes (12 equal slots)')
    ap.add_argument('--cycles', type=int, default=4, help='use/idle cycles of 5 + 5 minutes')
    ap.add_argument('--no-sensor', dest='sensor', action='store_false',
                    help='do not require OOK packets (no 433 MHz sensor nearby)')
    args = ap.parse_args()
    out = ROOT / 'output' / 'deck-stability' / time.strftime('%Y%m%d-%H%M%S')
    out.mkdir(parents=True)
    deck = Deck(args.deck, out, args.sensor)
    outcome, reason = 'incomplete', ''
    try:
        deck.say(f'output {out}')
        deck.collect(15)            # let a deck that rebooted on port open finish booting
        if deck.boots:
            deck.say('deck booted while the port opened; waiting for the link')
        deck.send('\ndebug on', .6)
        deadline = time.monotonic() + 60
        while True:
            pre = deck.take('preflight', check=False)
            if pre['link_ready'] and pre['sd_ready']:
                break
            if time.monotonic() > deadline:
                raise Fault('preflight: link or SD not ready after 60 s')
            deck.collect(2)
        deck.armed = True
        deck.say('preflight ' + deck.brief(pre))
        deck.stop_phy()
        deck.start_receivers()
        deck.start_mode(MODES[0])
        base = deck.take('all_lanes')
        deck.baseline = {k: base[k] for k in ('link_errors', 'link_timeouts', 'link_resets')}
        deck.say('all lanes up: ' + deck.brief(base))

        if args.all_lanes > 0:
            deck.run_until(time.monotonic() + args.all_lanes * 60, 'all_lanes', MODES[0])
            deck.record({'stage_complete': 'all_lanes', 'elapsed_s': deck.elapsed()})
            deck.say('stage all_lanes complete')

        if args.switching > 0:
            slot = args.switching * 60 / len(MODES)
            for i, mode in enumerate(MODES):
                deck.start_mode(mode)
                deck.say(f'switch {i + 1}/{len(MODES)} {mode[0]}')
                deck.run_until(time.monotonic() + slot, 'switch_' + mode[0], mode, tour_every=0)
            deck.record({'stage_complete': 'switching', 'elapsed_s': deck.elapsed()})
            deck.say('stage switching complete')

        picks = [MODES[i] for i in (0, 5, 2, 3)]
        for i in range(args.cycles):
            mode = picks[i % len(picks)]
            deck.expected = {}
            deck.start_receivers()
            deck.start_mode(mode)
            deck.say(f'use {i + 1}/{args.cycles} {mode[0]}')
            deck.run_until(time.monotonic() + 300, f'use_{i + 1}', mode)
            deck.expected = {}
            deck.stop_phy()
            deck.stop_receivers()
            deck.expected = {'lora_active': 0, 'ook_active': 0, 'probe_owner': 'none'}
            deck.send('card info', .3)
            deck.say(f'idle {i + 1}/{args.cycles}')
            deck.run_until(time.monotonic() + 300, f'idle_{i + 1}', tour_every=0)
        if args.cycles:
            deck.record({'stage_complete': 'use_idle', 'elapsed_s': deck.elapsed()})
        outcome = 'completed'
    except Stop:
        outcome = 'stopped'
    except Fault as exc:
        outcome, reason = 'fault', str(exc)
    finally:
        try:
            deck.expected = {}
            deck.send('stop phy', .6)
            deck.send('stop lora', .6)
            deck.send('stop ook', .6)
            deck.send('card info', .5)
            final = deck.take('finished', check=False)
            deck.say('final ' + deck.brief(final))
            deck.send('debug off', .5)
        except Exception as exc:  # noqa: BLE001 — cleanup must report, never mask the outcome
            deck.say('cleanup not confirmed: ' + str(exc))
        if deck.stop_file.exists():
            deck.stop_file.unlink()
        summary = {'outcome': outcome, 'reason': reason, 'events': deck.events, **summarize(deck.samples)}
        deck.record(summary)
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        deck.say(f'OUTCOME {outcome} {reason}')
        deck.say(json.dumps(summary))


if __name__ == '__main__':
    main()
