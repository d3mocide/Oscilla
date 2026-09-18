#!/usr/bin/env python3
"""ocp_repl.py — host-side OCP client.

    ocp_repl.py --selftest               assert OCP-SPEC §9 conformance
    ocp_repl.py --replay fixtures/x.txt  parse a canned byte stream
    ocp_repl.py /dev/ttyACM0             drive a real probe

Only the serial mode needs pyserial or hardware.

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ocp  # noqa: E402
from ocp import (  # noqa: E402
    Error,
    Event,
    Frame,
    Noise,
    OcpParser,
    Pong,
    encode_command,
    header_verbs,
)

# --- Presentation -----------------------------------------------------------

C = {
    "reset": "\033[0m", "dim": "\033[2m", "bold": "\033[1m",
    "red": "\033[31m", "green": "\033[32m", "yellow": "\033[33m",
    "blue": "\033[34m", "magenta": "\033[35m", "cyan": "\033[36m",
}


def safe(raw: bytes | str) -> str:
    """Re-escape a decoded field for display (OCP-SPEC §6).

    Decoded bytes are attacker-controlled; printing them raw lets a crafted
    SSID inject ANSI escapes into the operator's terminal.
    """
    if isinstance(raw, str):
        raw = raw.encode("utf-8", "surrogateescape")
    out = []
    for ch in raw.decode("utf-8", "replace"):
        cp = ord(ch)
        if ch in ("\t",) or (0x20 <= cp <= 0x7E) or (
            cp > 0xA0 and ch.isprintable()
        ):
            out.append(ch)
        else:
            out.append(f"\\x{cp:02x}" if cp < 0x100 else f"\\u{cp:04x}")
    return "".join(out)


def paint(text: str, *styles: str, enabled: bool = True) -> str:
    if not enabled or not styles:
        return text
    return "".join(C[s] for s in styles) + text + C["reset"]


def show(item, color: bool = True) -> None:
    """Pretty-print one parsed item."""
    p = lambda t, *s: paint(t, *s, enabled=color)  # noqa: E731

    if isinstance(item, Noise):
        why = f"  ({item.reason})" if item.reason != "not a marker" else ""
        print(p(f"  · {safe(item.text)}{why}", "dim"))
        return

    if isinstance(item, Pong):
        print(p("  pong", "green"))
        return

    if isinstance(item, Event):
        kv = " ".join(f"{k}={safe(v)}" for k, v in item.kv.items() if k != "kind")
        print(p(f"  ~ {safe(item.kind) or '?'}", "magenta"), p(kv, "dim"))
        return

    if isinstance(item, Error):
        extra = " ".join(
            f"{k}={safe(v)}" for k, v in item.kv.items() if k not in ("code", "msg")
        )
        print(p(f"  ! {safe(item.code)}", "red", "bold"), safe(item.msg),
              p(extra, "dim"))
        return

    if isinstance(item, Frame):
        kv = " ".join(f"{k}={safe(v)}" for k, v in item.kv.items())
        shape = "compact" if item.compact else f"{len(item.rows)} row(s)"
        print(p(f"  {item.tag}", "cyan", "bold"), kv, p(f"[{shape}]", "dim"))
        for row in item.rows:
            if row.startswith('"'):
                try:
                    fields = [safe(f) for f in ocp.split_csv_row(row)]
                    print("     ", p(" │ ".join(fields), "blue"))
                    continue
                except ocp.OcpFramingError:
                    pass
            print("     ", safe(row))


# --- Modes ------------------------------------------------------------------


def mode_replay(path: Path, color: bool = True, chunk: int = 7) -> int:
    """Parse a canned byte stream, in odd chunks to exercise line assembly."""
    data = path.read_bytes()
    parser = OcpParser()
    counts = {"frames": 0, "events": 0, "errors": 0, "noise": 0, "pong": 0}

    print(paint(f"replay {path} ({len(data)} bytes, {chunk}-byte chunks)", "bold",
                enabled=color))
    for i in range(0, len(data), chunk):
        for item in parser.feed_bytes(data[i : i + chunk]):
            show(item, color)
            match item:
                case Frame():
                    counts["frames"] += 1
                case Event():
                    counts["events"] += 1
                case Error():
                    counts["errors"] += 1
                case Pong():
                    counts["pong"] += 1
                case Noise():
                    counts["noise"] += 1

    if parser.frame_open:
        left = parser.abandon_open_frame()
        print(paint(f"  (abandoned open {left.tag} frame at EOF)", "yellow",
                    enabled=color))

    print(paint("  " + "  ".join(f"{k}={v}" for k, v in counts.items()), "dim",
                enabled=color))
    return 0


class Probe:
    """A live probe: background reader, parsed items on a queue."""

    def __init__(self, port: str, baud: int):
        import queue
        import serial  # type: ignore
        import threading

        self.ser = serial.Serial(port, baud, timeout=0.05)
        self.parser = OcpParser()
        self.q: "queue.Queue" = queue.Queue()
        self._stop = threading.Event()
        self._t = threading.Thread(target=self._read, daemon=True)
        self._t.start()
        # A reset can leave half a line in the probe's buffer; end it so it
        # cannot prefix our first command (OCP-SPEC §2: blank lines are ignored).
        self.ser.write(b"\n")

    def _read(self) -> None:
        while not self._stop.is_set():
            data = self.ser.read(256)
            if data:
                for item in self.parser.feed_bytes(data):
                    self.q.put(item)

    def send(self, line: str) -> None:
        parts = line.split()
        self.ser.write(encode_command(parts[0], *parts[1:]))

    def collect(self, seconds: float) -> list:
        import queue
        out, deadline = [], time.time() + seconds
        while time.time() < deadline:
            try:
                out.append(self.q.get(timeout=max(0.01, deadline - time.time())))
            except queue.Empty:
                break
        return out

    def command(self, line: str, wait: float = 1.0) -> list:
        self.send(line)
        return self.collect(wait)

    def close(self) -> None:
        self._stop.set()
        self._t.join(timeout=1.0)
        self.ser.close()


def mode_exec(port: str, baud: int, cmds: list[str], color: bool = True) -> int:
    """Run commands non-interactively and print what comes back."""
    p = Probe(port, baud)
    try:
        for item in p.collect(0.6):          # catch a boot [HELLO]
            show(item, color)
        for cmd in cmds:
            print(paint(f"> {cmd}", "bold", enabled=color))
            for item in p.command(cmd, wait=1.2):
                show(item, color)
    finally:
        p.close()
    return 0


def mode_gate(port: str, baud: int, color: bool = True) -> int:
    """P1 exit gate: drive the probe through the system verbs and assert."""
    checks: list[tuple[str, bool, str]] = []

    def check(name: str, ok, detail: str = "") -> None:
        checks.append((name, None if ok is None else bool(ok), detail))

    def first(items, cls, tag=None):
        for i in items:
            if isinstance(i, cls) and (tag is None or getattr(i, "tag", None) == tag):
                return i
        return None

    p = Probe(port, baud)
    try:
        # A freshly reset probe enumerates before its app is up; retry, bounded.
        hello = None
        for _ in range(8):
            hello = first(p.command("hello", 0.75), Frame, "[HELLO]")
            if hello:
                break
        check("hello returns [HELLO]", hello is not None)
        if hello:
            check("proto matches ocp.h",
                  hello.get("proto") == str(ocp.PROTO_VERSION), hello.get("proto") or "")
            check("fw identifies the probe", hello.get("fw") == "oscilla-c5",
                  hello.get("fw") or "")
            check("caps key present", "caps" in hello.kv)

        check("ping returns pong", first(p.command("ping"), Pong) is not None)

        ver = first(p.command("version"), Frame, "[VER]")
        check("version returns [VER]", ver is not None,
              (ver.get("ver") or "") if ver else "")

        st = first(p.command("status"), Frame, "[STATUS]")
        check("status returns [STATUS]", st is not None)
        if st:
            check("status reports an idle PHY owner", st.get("owner") == "none",
                  st.get("owner") or "")
            check("status reports uptime", (st.get("uptime_ms") or "").isdigit())

        stop = first(p.command("stop"), Frame, "[STOP]")
        check("stop is acked when idle", stop is not None and stop.get("running") == "0")

        err = first(p.command("no_such_verb"), Error)
        check("unknown verb -> code=unknown",
              err is not None and err.code == "unknown", err.code if err else "")

        caps = set(filter(None, (hello.get("caps") or "").split(","))) if hello else set()
        absent = ocp.verb_unsupported_by(caps)
        if absent:
            err = first(p.command(absent), Error)
            check(f"verb without its cap ({absent}) -> code=nocap",
                  err is not None and err.code == "nocap", err.code if err else "")
        else:
            check("verb without its cap -> nocap", None, "every cap present")

        err = first(p.command("inspect_network"), Error)
        check("wrong arity -> code=badarg",
              err is not None and err.code == "badarg", err.code if err else "")

        # An over-long line must be refused without desynchronising the next.
        p.ser.write(b"x" * (ocp.MAX_LINE_LEN + 200) + b"\n")
        long_err = first(p.collect(1.0), Error)
        check("over-long line -> code=badarg", long_err is not None
              and long_err.code == "badarg", long_err.code if long_err else "")
        check("probe still answers after an over-long line",
              first(p.command("ping"), Pong) is not None)

        # Reset announcement: reboot must produce a fresh unsolicited [HELLO],
        # and boot chatter must never be mistaken for a frame.
        p.send("reboot")
        items = p.collect(4.0)
        check("reboot announces an unsolicited [HELLO]",
              first(items, Frame, "[HELLO]") is not None)
        check("boot text parsed as noise, never as a frame",
              all(not (isinstance(i, Frame) and i.tag != "[HELLO]") for i in items),
              f"{sum(isinstance(i, Noise) for i in items)} noise lines")
        check("probe usable after reset", first(p.command("ping", 2.0), Pong) is not None)
    finally:
        p.close()

    return report(checks, "P1 probe exit gate", color)


def report(checks: list, title: str, color: bool) -> int:
    """ok is True (PASS), False (FAIL) or None (SKIP: not exercised, not a pass)."""
    width = max(len(n) for n, _, _ in checks)
    for name, ok, detail in checks:
        mark = (paint("SKIP", "yellow", enabled=color) if ok is None else
                paint("PASS", "green", enabled=color) if ok else
                paint("FAIL", "red", "bold", enabled=color))
        print(f"  {mark}  {name.ljust(width)}  {paint(detail, 'dim', enabled=color) if detail else ''}")
    failed = sum(ok is False for _, ok, _ in checks)
    skipped = sum(ok is None for _, ok, _ in checks)
    passed = len(checks) - failed - skipped
    tail = f", {skipped} skipped" if skipped else ""
    print()
    if failed:
        print(paint(f"{failed} FAILED, {passed} passed{tail} — {title}", "red", "bold", enabled=color))
    else:
        print(paint(f"{passed} passed{tail} — {title}", "green", "bold", enabled=color))
    return 1 if failed else 0


def wait_for(p: "Probe", pred, timeout: float) -> list:
    """Collect items until pred(items) is true or timeout; returns all items."""
    got, deadline = [], time.time() + timeout
    while time.time() < deadline:
        got.extend(p.collect(0.2))
        if pred(got):
            break
    return got


def mode_gate_wifi(port: str, baud: int, color: bool = True) -> int:
    """P2 Wi-Fi checks against a live probe. Prints counts only, never SSIDs
    or BSSIDs: scan results are field data (SECURITY.md)."""
    checks: list[tuple[str, bool, str]] = []

    def check(name, ok, detail=""):
        checks.append((name, None if ok is None else bool(ok), detail))

    def frames(items, tag):
        return [i for i in items if isinstance(i, Frame) and i.tag == tag]

    def errors(items):
        return [i for i in items if isinstance(i, Error)]

    p = Probe(port, baud)
    try:
        p.collect(0.5)
        hello = frames(p.command("hello", 1.5), "[HELLO]")
        caps = hello[0].get("caps", "") if hello else ""
        check("probe advertises wifi24 and wifi5", "wifi24" in caps and "wifi5" in caps, caps)

        # A full scan.
        t0 = time.time()
        p.send("scan_networks")
        mid = p.command("status", 1.0)
        got = wait_for(p, lambda it: frames(it, "[SCAN]") or errors(it), 60)
        scan = frames(mid + got, "[SCAN]")
        status_mid = frames(mid, "[STATUS]")
        busy = [e for e in errors(mid) if e.code == "busy"]
        check("status during a scan reports owner=wifi (or busy)",
              (status_mid and status_mid[0].get("owner") == "wifi") or busy,
              status_mid[0].get("owner") if status_mid else "busy")
        check("scan_networks replies with [SCAN]", scan, f"{time.time() - t0:.1f}s")
        if scan:
            f = scan[0]
            n, total, first = int(f.get("n", "-1")), int(f.get("total", "-1")), int(f.get("first", "-1"))
            rows = f.csv_rows()
            check("n matches the rows delivered", n == len(rows), f"n={n} rows={len(rows)}")
            check("first page starts at 1, n <= total, n <= cap",
                  first == 1 and n <= total and n <= ocp.MAX_FRAME_ROWS, f"total={total}")
            check("every row has 7 columns", all(len(r) == 7 for r in rows))
            check("rows are numbered consecutively from first",
                  [int(r[0]) for r in rows] == list(range(first, first + n)))
            check("rows are in descending RSSI order",
                  all(int(a[5]) >= int(b[5]) for a, b in zip(rows, rows[1:])))
            check("band column agrees with channel",
                  all((r[6] == b"2.4") == (int(r[3]) <= 14) for r in rows))
            check("auth column is always a known label",
                  all(r[4].decode() in AUTH_LABELS for r in rows))
            bands = {r[6] for r in rows}
            check("both bands heard", bands >= {b"2.4", b"5"}, f"{sum(r[6]==b'2.4' for r in rows)} / {sum(r[6]==b'5' for r in rows)}")
            check("no leftover PHY owner after the scan",
                  frames(p.command("status"), "[STATUS]")[0].get("owner") == "none")

            if total >= 2:
                mid_idx = total // 2 + 1
                page = frames(p.command(f"show_scan_results {mid_idx}", 2.0), "[SCAN]")
                ok = page and int(page[0].get("first")) == mid_idx and \
                     int(page[0].get("n")) == min(total - mid_idx + 1, ocp.MAX_FRAME_ROWS)
                check("show_scan_results <first> returns the right page", ok)
            e = errors(p.command(f"show_scan_results {total + 1}"))
            check("page past the end -> badarg", e and e[0].code == "badarg")
            e = errors(p.command("show_scan_results 0"))
            check("page 0 -> badarg (indices are 1-based)", e and e[0].code == "badarg")

            # inspect_network: one AP per band, cross-checked against the scan row.
            by_band = {}
            for r in rows:
                by_band.setdefault(r[6], r)
            targets = sorted(by_band.items())
            # Also the first WPA3-only AP: the one case that must report mfp_required=1.
            wpa3 = next((r for r in rows if r[4] in (b"WPA3", b"WPA3-EAP", b"WPA3-EAP192")), None)
            if wpa3 is not None and all(wpa3[0] != r[0] for _, r in targets):
                targets.append((wpa3[6], wpa3))
            if wpa3 is None:
                check("WPA3-only AP in range (mfp_required path)", None, "none in range")
            for band, r in targets:
                idx = int(r[0])
                t1 = time.time()
                p.send(f"inspect_network {idx}")
                mid = p.command("status", 0.6)
                # The reply may already be in `mid`: don't wait for what has arrived.
                got = [] if frames(mid, "[INSPECT]") else \
                    wait_for(p, lambda it: frames(it, "[INSPECT]") or errors(it), 6)
                elapsed = time.time() - t1
                ins = frames(mid + got, "[INSPECT]")
                label = f"{band.decode()} GHz{' WPA3' if r is wpa3 else ''}"
                st = frames(mid, "[STATUS]")
                check(f"[{label}] status mid-inspect reports owner=wifi",
                      st and st[0].get("owner") == "wifi")
                check(f"[{label}] inspect replies with [INSPECT]", ins, f"<= {elapsed:.1f}s")
                if not ins:
                    continue
                f = ins[0]
                check(f"[{label}] idx/bssid/ch/band match the scan row",
                      f.get("idx") == str(idx) and f.get("bssid", "").encode() == r[2]
                      and f.get("ch", "").encode() == r[3] and f.get("band", "").encode() == r[6])
                kv = f.row_kvs()[0] if f.rows else {}
                beacons = int(kv.get("beacons", b"-1"))
                check(f"[{label}] beacons heard", beacons > 0, f"beacons={beacons}")
                if beacons > 0:
                    rsn = kv.get("rsn"); cap = kv.get("mfp_capable"); req = kv.get("mfp_required")
                    check(f"[{label}] rsn/mfp fields are 0 or 1",
                          {rsn, cap, req} <= {b"0", b"1"})
                    check(f"[{label}] interval_ms plausible (10-1000)",
                          10 <= int(kv.get("interval_ms", b"0")) <= 1000, kv.get("interval_ms", b"").decode())
                    auth = r[4].decode()
                    if auth == "OPEN":
                        check(f"[{label}] OPEN network has no RSN", rsn == b"0")
                    elif auth in ("WPA3", "WPA3-EAP", "WPA3-EAP192"):
                        check(f"[{label}] WPA3-only network requires MFP", req == b"1")
                    elif auth.startswith("WPA2/WPA3"):
                        check(f"[{label}] WPA2/WPA3 transition network is MFP-capable", cap == b"1")
                    else:
                        check(f"[{label}] RSN present for {auth}", rsn == b"1" or auth in ("WEP", "UNKNOWN"), auth)
                check(f"[{label}] PHY released after inspect",
                      frames(p.command("status"), "[STATUS]")[0].get("owner") == "none")

            e = errors(p.command("inspect_network 0"))
            check("inspect idx 0 -> badarg", e and e[0].code == "badarg")
            e = errors(p.command(f"inspect_network {total + 1}"))
            check("inspect idx past the results -> badarg", e and e[0].code == "badarg")

            # A capture can finish in ~0.3 s, so stop may win or lose the race.
            # Either outcome must be consistent; the abort path must be seen.
            aborts, consistent = 0, True
            for _ in range(3):
                p.send("inspect_network 1")
                p.send("stop")
                got = wait_for(p, lambda it: frames(it, "[STOP]"), 5)
                order = [i.tag for i in got if isinstance(i, Frame) and i.tag in ("[INSPECT]", "[STOP]")]
                ins, st = frames(got, "[INSPECT]"), frames(got, "[STOP]")
                aborted = bool(ins) and ins[0].get("aborted") == "1"
                running = bool(st) and st[0].get("running") == "1"
                consistent &= order == ["[INSPECT]", "[STOP]"] and aborted == running
                aborts += aborted
                p.collect(0.3)
            check("stop vs inspect: [INSPECT] always before [STOP], aborted <=> running=1", consistent)
            check("stop mid-inspect takes the abort path", aborts > 0, f"{aborts}/3 aborted")

        # stop mid-scan: aborted [SCAN] then [STOP] running=1, in that order.
        p.send("scan_networks")
        time.sleep(2.0)
        e = errors(p.command("scan_networks", 1.0))
        check("second scan while scanning -> busy", e and e[0].code == "busy")
        p.send("stop")
        got = wait_for(p, lambda it: frames(it, "[STOP]"), 5)
        order = [i.tag for i in got if isinstance(i, Frame) and i.tag in ("[SCAN]", "[STOP]")]
        sc, st = frames(got, "[SCAN]"), frames(got, "[STOP]")
        check("stop mid-scan: aborted [SCAN] arrives before [STOP]", order == ["[SCAN]", "[STOP]"], str(order))
        check("aborted frame is flagged and empty",
              sc and sc[0].get("aborted") == "1" and sc[0].get("n") == "0")
        check("[STOP] running=1", st and st[0].get("running") == "1")
        st = frames(p.command("stop"), "[STOP]")
        check("stop when idle -> running=0", st and st[0].get("running") == "0")
        check("probe still answers after all that", any(isinstance(i, Pong) for i in p.command("ping")))
    finally:
        p.close()

    return report(checks, "P2 Wi-Fi", color)


MAC_RE = re.compile(r"^([0-9a-f]{2}:){5}[0-9a-f]{2}$")


def mode_gate_stop(port: str, baud: int, color: bool = True) -> int:
    """Scoped-`stop` checks against a live probe (D-16, OCP-SPEC §5.4).

    The load-bearing one is the cross-lane isolation: `stop phy` must release
    the PHY lane and leave a running LoRa RX session alone. It can only be
    confirmed with both radios live."""
    checks: list[tuple[str, bool, str]] = []

    def check(name, ok, detail=""):
        checks.append((name, None if ok is None else bool(ok), detail))

    def frames(items, tag):
        return [i for i in items if isinstance(i, Frame) and i.tag == tag]

    def errors(items):
        return [i for i in items if isinstance(i, Error)]

    p = Probe(port, baud)
    try:
        p.collect(0.5)

        # Arity and validation come first: they need no radios at all.
        st = frames(p.command("stop"), "[STOP]")
        check("bare stop is still acked", st, "")
        check("bare stop reports lane=all", st and st[0].get("lane") == "all",
              st[0].get("lane") if st else "")

        for lane in ("phy", "lora", "all"):
            st = frames(p.command(f"stop {lane}"), "[STOP]")
            check(f"stop {lane} is acked and echoes its lane",
                  st and st[0].get("lane") == lane, st[0].get("lane") if st else "")

        errs = errors(p.command("stop wifi"))
        check("an unknown lane is badarg, not a silent global stop",
              errs and errs[0].code == "badarg",
              errs[0].code if errs else "no [ERR]")
        errs = errors(p.command("stop phy lora"))
        check("two lanes is badarg (max_args=1)",
              errs and errs[0].code == "badarg",
              errs[0].code if errs else "no [ERR]")

        st = frames(p.command("status"), "[STATUS]")
        lora_present = st and st[0].get("lora") != "absent"
        if not lora_present:
            check("cross-lane isolation: stop phy spares LoRa", None,
                  "no SX1262 on this probe")
            check("cross-lane isolation: stop lora spares the PHY", None,
                  "no SX1262 on this probe")
        else:
            # Both lanes up at once — the configuration the bug needed.
            # cr is the coding-rate *index* 1-4 (4/5..4/8, ocp_server.c), not
            # the raw "5" in "4/5" - confirmed against lora_recon.c after this
            # got it wrong once and every cross-lane check failed as a result.
            cfg_err = errors(p.command("lora_config 915000000 7 125 1"))
            if cfg_err:
                check("lora_config accepted (needed for the cross-lane checks)", False,
                      f"{cfg_err[0].code}: {cfg_err[0].msg}")
            frames(p.command("lora_listen", 1.0), "[LORA]")
            frames(p.command("start_sniffer", 1.5), "[SNIFF]")

            st = frames(p.command("status", 0.6), "[STATUS]")
            both = st and st[0].get("owner") == "wifi" and st[0].get("lora") == "rx"
            check("both lanes run concurrently (owner=wifi, lora=rx)", both,
                  f"owner={st[0].get('owner')} lora={st[0].get('lora')}" if st else "")

            st = frames(p.command("stop phy"), "[STOP]")
            check("stop phy reports something was running",
                  st and st[0].get("running") == "1", "")
            st = frames(p.command("status", 0.6), "[STATUS]")
            check("cross-lane isolation: stop phy spares LoRa",
                  st and st[0].get("owner") == "none" and st[0].get("lora") == "rx",
                  f"owner={st[0].get('owner')} lora={st[0].get('lora')}" if st else "")

            # And the mirror image.
            frames(p.command("start_sniffer", 1.5), "[SNIFF]")
            st = frames(p.command("stop lora"), "[STOP]")
            check("stop lora reports something was running",
                  st and st[0].get("running") == "1", "")
            st = frames(p.command("status", 0.6), "[STATUS]")
            check("cross-lane isolation: stop lora spares the PHY",
                  st and st[0].get("owner") == "wifi" and st[0].get("lora") == "idle",
                  f"owner={st[0].get('owner')} lora={st[0].get('lora')}" if st else "")

            st = frames(p.command("stop"), "[STOP]")
            check("a bare stop still clears both lanes",
                  st and st[0].get("running") == "1", "")
            st = frames(p.command("status", 0.6), "[STATUS]")
            check("both lanes idle after a bare stop",
                  st and st[0].get("owner") == "none" and st[0].get("lora") == "idle",
                  f"owner={st[0].get('owner')} lora={st[0].get('lora')}" if st else "")

        check("probe still answers after all that",
              any(isinstance(i, Pong) for i in p.command("ping")))
    finally:
        p.close()

    return report(checks, "D-16 scoped stop", color)


def mode_gate_sniffer(port: str, baud: int, color: bool = True) -> int:
    """Promiscuous-sniffer checks against a live probe (P7, started early —
    see WORKLOG). Prints counts and channel/RSSI only, never MACs or SSIDs:
    sniffer output is field data (SECURITY.md), more so than a scan result.

    Table population (real client/probe rows) depends on real RF traffic
    during a short automated run, so those checks SKIP rather than FAIL on
    an empty table instead of asserting devices exist nearby."""
    checks: list[tuple[str, bool, str]] = []

    def check(name, ok, detail=""):
        checks.append((name, None if ok is None else bool(ok), detail))

    def frames(items, tag):
        return [i for i in items if isinstance(i, Frame) and i.tag == tag]

    def errors(items):
        return [i for i in items if isinstance(i, Error)]

    def events(items, kind):
        return [i for i in items if isinstance(i, Event) and i.kind == kind]

    def ev_int(e: "Event", key: str, default: int = -1) -> int:
        v = e.kv.get(key)
        return int(v) if v and v.isdigit() else default

    p = Probe(port, baud)
    try:
        p.collect(0.5)

        sniff = frames(p.command("start_sniffer", 1.5), "[SNIFF]")
        check("start_sniffer replies with [SNIFF]", sniff, "")
        if sniff:
            ch = sniff[0].get("ch")
            check("[SNIFF] ch is a plausible channel number",
                  (ch or "").isdigit() and 1 <= int(ch) <= 196, ch or "")

        st = frames(p.command("status", 0.6), "[STATUS]")
        check("status mid-sniff reports owner=wifi", st and st[0].get("owner") == "wifi",
              st[0].get("owner") if st else "")

        e = errors(p.command("start_sniffer", 1.0))
        check("start_sniffer while already running -> busy", e and e[0].code == "busy")

        # Let it hop for a while: waiting for ~one full sweep of both bands
        # (22 channels * SNIFF_DWELL_MS ~ 6.6s) gives the client/probe checks
        # below a real, if still not guaranteed, chance at nearby traffic.
        got = wait_for(p, lambda it: len(events(it, "sniff")) >= 20, 12.0)
        sniffs = events(got, "sniff")
        check("[EVT] kind=sniff arrives while hopping", sniffs, f"{len(sniffs)} in 10s")
        if sniffs:
            chans = [ev_int(e, "ch") for e in sniffs]
            check("every sniff event's ch is a plausible channel number",
                  all(1 <= c <= 196 for c in chans), str(chans[:6]))
            pkts = [ev_int(e, "pkts") for e in sniffs]
            check("pkts is a non-decreasing session counter",
                  all(a <= b for a, b in zip(pkts, pkts[1:])), str(pkts[:6]))

        clients = events(got, "client")
        probes = events(got, "probe")
        check("[EVT] kind=client seen (needs a real AP+client on an overlapping channel)",
              None if not clients else True, f"{len(clients)} in the window")
        check("[EVT] kind=probe seen (needs a real device probing nearby)",
              None if not probes else True, f"{len(probes)} in the window")

        cf = frames(p.command("show_clients", 1.5), "[CLIENTS]")
        check("show_clients replies with [CLIENTS]", cf, "")
        if cf:
            n, rows = int(cf[0].get("n", "-1")), cf[0].csv_rows()
            check("n matches the rows delivered", n == len(rows), f"n={n} rows={len(rows)}")
            if rows:
                check("every row has 6 columns", all(len(r) == 6 for r in rows))
                check("bssid/mac columns look like MAC addresses",
                      all(MAC_RE.match(r[0].decode()) and MAC_RE.match(r[1].decode()) for r in rows))
                check("band column agrees with channel",
                      all((r[3] == b"2.4") == (int(r[2]) <= 14) for r in rows))
            else:
                check("at least one AP<->client pairing captured", None, "none in range/window")

        pf = frames(p.command("show_probes", 1.5), "[PROBES]")
        check("show_probes replies with [PROBES]", pf, "")
        if pf:
            n, rows = int(pf[0].get("n", "-1")), pf[0].csv_rows()
            check("n matches the rows delivered", n == len(rows), f"n={n} rows={len(rows)}")
            if rows:
                check("every row has 4 columns", all(len(r) == 4 for r in rows))
                check("mac column looks like a MAC address", all(MAC_RE.match(r[0].decode()) for r in rows))
            else:
                check("at least one probe-request pairing captured", None, "none in range/window")

        # stop: unlike scan/inspect there is no open frame to abort, so [STOP]
        # should land quickly with no preceding aborted-frame race.
        t0 = time.time()
        p.send("stop")
        got = wait_for(p, lambda it: frames(it, "[STOP]"), 5)
        st = frames(got, "[STOP]")
        check("stop lands promptly", st and time.time() - t0 < 3.0, f"{time.time() - t0:.1f}s")
        check("[STOP] running=1", st and st[0].get("running") == "1")

        st = frames(p.command("status"), "[STATUS]")
        check("PHY released after stop (owner=none)", st and st[0].get("owner") == "none")

        # A fresh session starts from an empty table, not the last session's.
        frames(p.command("start_sniffer", 1.0), "[SNIFF]")
        p.send("stop")
        wait_for(p, lambda it: frames(it, "[STOP]"), 3)

        st = frames(p.command("stop"), "[STOP]")
        check("stop when idle -> running=0", st and st[0].get("running") == "0")
        check("probe still answers after all that", any(isinstance(i, Pong) for i in p.command("ping")))
    finally:
        p.close()

    return report(checks, "P7 sniffer (early)", color)


AUTH_LABELS = {v for k, v in ocp.read_header_defines().items() if k.startswith("OCP_AUTH_")}


def mode_serial(port: str, baud: int, color: bool = True) -> int:
    try:
        import serial  # type: ignore
    except ImportError:
        print("error: serial mode needs pyserial  (pip install pyserial)",
              file=sys.stderr)
        return 2

    import select
    import threading

    verbs = header_verbs()
    parser = OcpParser()

    with serial.Serial(port, baud, timeout=0.05) as ser:
        print(paint(f"OCP on {port} @ {baud} 8N1 — proto {ocp.PROTO_VERSION}",
                    "bold", enabled=color))
        print(paint("type a verb, '?' for the command table, Ctrl-D to quit",
                    "dim", enabled=color))

        stop = threading.Event()

        def reader() -> None:
            while not stop.is_set():
                data = ser.read(256)
                if data:
                    for item in parser.feed_bytes(data):
                        show(item, color)

        t = threading.Thread(target=reader, daemon=True)
        t.start()

        # The probe may be mid-boot; solicit a [HELLO] so caps are known.
        time.sleep(0.3)
        ser.write(encode_command("hello"))

        try:
            while True:
                try:
                    line = input(paint("> ", "bold", enabled=color)).strip()
                except EOFError:
                    break
                if not line:
                    continue
                if line in ("?", "help"):
                    print(paint("  " + "  ".join(verbs), "dim", enabled=color))
                    continue
                if line in ("quit", "exit"):
                    break
                parts = line.split()
                if parts[0] not in verbs:
                    print(paint(f"  ! '{parts[0]}' is not in this build's command "
                                f"table (? to list)", "yellow", enabled=color))
                    continue
                try:
                    ser.write(encode_command(parts[0], *parts[1:]))
                except ValueError as exc:
                    print(paint(f"  ! {exc}", "red", enabled=color))
        finally:
            stop.set()
            t.join(timeout=1.0)
    return 0


# --- Self-test: protocol/OCP-SPEC.md §9 conformance checklist ---------------


def mode_selftest(color: bool = True) -> int:
    """Assert every item on the OCP-SPEC §9 conformance checklist."""
    checks: list[tuple[str, bool, str]] = []

    def check(name: str, ok: bool, detail: str = "") -> None:
        checks.append((name, bool(ok), detail))

    def items(*lines: str) -> list:
        p = OcpParser()
        out = []
        for ln in lines:
            out.extend(p.feed_line(ln))
        return out

    drift = ocp.check_against_header()
    check("ocp.py literals match ocp.h", not drift, "; ".join(drift))

    # No transmit verb (D-8).
    tx = [v for v in header_verbs()
          if any(n in v for n in ("_tx", "tx_", "transmit", "send", "inject"))]
    check("no transmit verb in the command table", not tx, ", ".join(tx))

    # Non-marker text never advances frame state.
    got = items("ets Jul 29 2019", "rst:0x1 (POWERON)", "I (31) boot: ok")
    check("boot chatter parses as noise only",
          len(got) == 3 and all(isinstance(i, Noise) for i in got))

    # An oversized line must not desynchronise the next.
    p = OcpParser(max_line_len=64)
    over = b"[SCAN] " + b"A" * 200 + b"\n[STOP] running=0 END\n"
    got = list(p.feed_bytes(over))
    check("oversized line discarded, next line still parses",
          len(got) == 2 and isinstance(got[0], Noise)
          and isinstance(got[1], Frame) and got[1].tag == "[STOP]")

    # [EVT]/[ERR] inside an open frame are routed out-of-band.
    got = items("[SCAN] BEGIN n=1",
                "[EVT] kind=sniff pkts=9",
                '[SCAN] "1","a","AA:BB:CC:DD:EE:01","6","WPA2","-50","2.4"',
                "[ERR] code=busy msg=\"x\"",
                "[SCAN] END")
    frames = [i for i in got if isinstance(i, Frame)]
    check("EVT/ERR inside a frame do not close it",
          len(got) == 3
          and isinstance(got[0], Event) and isinstance(got[1], Error)
          and len(frames) == 1 and len(frames[0].rows) == 1)

    # Unknown marker / kind / key are ignored, never fatal.
    got = items("[QUANTUM] BEGIN a=1", "[QUANTUM] row", "[QUANTUM] END")
    check("unknown marker parses as an ordinary frame",
          len(got) == 1 and isinstance(got[0], Frame) and got[0].tag == "[QUANTUM]")
    got = items("[EVT] kind=teleport distance=9000")
    check("unknown EVT kind is delivered, not fatal",
          len(got) == 1 and isinstance(got[0], Event) and got[0].kind == "teleport")
    got = items("[STOP] running=0 future_key=7 END")
    check("unknown k=v key is ignored",
          len(got) == 1 and got[0].get("running") == "0")

    # Compact and block forms accepted for any tag.
    compact = items("[STOP] running=0 END")
    block = items("[STOP] BEGIN", "[STOP] running=0", "[STOP] END")
    check("compact and block forms both accepted",
          len(compact) == 1 and compact[0].compact
          and len(block) == 1 and not block[0].compact
          and block[0].row_kvs()[0]["running"] == b"0")

    # Escapes round-trip byte-exactly.
    payloads = [b"plain", b"", b'quote" and \\ backslash',
                b"newline\nand\rCR", b"caf\xc3\xa9", b"\x00\x01\xff",
                b"comma,separated", "日本語".encode()]
    bad = [p for p in payloads if ocp.decode_field(ocp.encode_field(p)) != p]
    check("escapes round-trip byte-exactly", not bad, repr(bad))

    # A newline in an SSID cannot break out of its row.
    evil = ocp.encode_field(b"evil\nrow")
    check("embedded newline cannot escape a field", "\n" not in evil, repr(evil))

    row = ",".join(ocp.encode_field(f) for f in
                   [b"1", b'a,b"c', b"AA:BB:CC:DD:EE:01", b"6", b"WPA2", b"-50", b"2.4"])
    got = items(f"[SCAN] BEGIN n=1", f"[SCAN] {row}", "[SCAN] END")
    fields = got[0].csv_rows()[0] if got and isinstance(got[0], Frame) else []
    check("CSV row splits on real separators only",
          len(fields) == 7 and fields[1] == b'a,b"c', repr(fields))

    # [HELLO] is delivered even inside an open frame.
    got = items("[LORA] BEGIN state=idle",
                "[LORA] freq=0",
                "ets Jul 29 2019",
                "[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,ble END")
    hellos = [i for i in got if isinstance(i, Frame) and i.tag == "[HELLO]"]
    check("HELLO inside an open frame is still delivered",
          len(hellos) == 1 and hellos[0].get("caps") == "wifi24,ble")

    # A frame missing its END is abandoned without wedging the reader.
    p = OcpParser()
    list(p.feed_line("[SCAN] BEGIN n=1"))
    abandoned = p.abandon_open_frame()
    after = list(p.feed_line("[STOP] running=0 END"))
    check("abandoning a half-read frame leaves the reader usable",
          abandoned is not None and abandoned.tag == "[SCAN]"
          and len(after) == 1 and after[0].tag == "[STOP]")

    # A malformed escape in k=v is noise, never an exception, and a malformed
    # [HELLO] must not abandon an open frame.
    try:
        got = items('[STOP] k="\\q" END', '[EVT] kind=x v="\\xZZ"')
        check("malformed escape in k=v is noise, not an exception",
              len(got) == 2 and all(isinstance(i, Noise) for i in got))
    except Exception as exc:  # noqa: BLE001
        check("malformed escape in k=v is noise, not an exception", False, repr(exc))
    p = OcpParser()
    list(p.feed_line("[SCAN] BEGIN n=1"))
    list(p.feed_line('[HELLO] proto="\\q" END'))
    check("malformed [HELLO] does not abandon an open frame", p.frame_open)
    got = items('[STOP] k="\\x+f" END', '[STOP] k="\\x f" END')
    check("\\xHH is strict: sign or space is malformed",
          len(got) == 2 and all(isinstance(i, Noise) for i in got))

    # Only SP/HT are whitespace: \x0c is part of the token, not a separator.
    got = items("\x0c[STOP] running=0 END")
    check("form feed is not whitespace (byte-exact, OCP-SPEC §2)",
          len(got) == 1 and isinstance(got[0], Noise))

    # A late bare END is noise, not an empty result.
    got = items("[SCAN] END")
    check("bare END with no open frame is noise, not an empty frame",
          len(got) == 1 and isinstance(got[0], Noise))

    # Row cap: an endless frame is dropped whole and the reader recovers.
    p = OcpParser(max_frame_rows=4)
    got = list(p.feed_line("[SCAN] BEGIN"))
    for _ in range(10):
        got += list(p.feed_line('[SCAN] "x"'))
    got += list(p.feed_line("[SCAN] END")) + list(p.feed_line("[STOP] running=0 END"))
    frames = [i for i in got if isinstance(i, Frame)]
    check("over-long frame abandoned whole; reader recovers",
          not p.frame_open and len(frames) == 1 and frames[0].tag == "[STOP]",
          f"frames={[f.tag for f in frames]}")

    # A re-opened frame does not nest.
    got = items("[SCAN] BEGIN n=1", "[SCAN] BEGIN n=2", "[SCAN] END")
    check("a re-opened frame is reported, not nested",
          any(isinstance(i, Noise) for i in got)
          and len([i for i in got if isinstance(i, Frame)]) == 1)

    # pong is recognised.
    got = items("pong")
    check("pong is recognised as ping's reply",
          len(got) == 1 and isinstance(got[0], Pong))
    got = items("pong extra")
    check("'pong extra' is not a pong",
          len(got) == 1 and isinstance(got[0], Noise))

    # Malformed input never raises out of the parser.
    for junk in ['[SCAN] "unterminated', "[SCAN] BEGIN k=\\xZZ", "[", "[]", "[ ]",
                 "[scan] lower", "\x00\x01\x02", "[SCAN]", "=", "==", "k=", '"'*5]:
        try:
            list(OcpParser().feed_line(junk))
        except Exception as exc:  # noqa: BLE001
            check(f"malformed input {junk!r} does not raise", False, repr(exc))
            break
    else:
        check("malformed input never raises out of the parser", True)

    try:
        encode_command("scan_bt", "10")
        encode_command("inspect_network", "1")
        ok = encode_command("set_channels", "1,6,11") == b"set_channels 1,6,11\n"
    except Exception as exc:  # noqa: BLE001
        ok = False
    check("command encoding round-trips", ok)
    try:
        encode_command("x", *["a"] * ocp.MAX_ARGV)
        ok = False
    except ValueError:
        ok = True
    check("command encoder enforces MAX_ARGV", ok)

    # The shipped fixture parses end to end.
    fixture = Path(__file__).resolve().parent / "fixtures" / "boot_and_scan.txt"
    if fixture.exists():
        p = OcpParser()
        got = []
        data = fixture.read_bytes()
        for i in range(0, len(data), 3):
            got.extend(p.feed_bytes(data[i : i + 3]))
        n_frames = len([i for i in got if isinstance(i, Frame)])
        n_noise = len([i for i in got if isinstance(i, Noise)])
        check("shipped fixture parses (frames found, noise tolerated)",
              n_frames >= 6 and n_noise >= 5, f"frames={n_frames} noise={n_noise}")

    width = max(len(n) for n, _, _ in checks)
    failed = 0
    for name, ok, detail in checks:
        mark = paint("PASS", "green", enabled=color) if ok else paint("FAIL", "red", "bold", enabled=color)
        print(f"  {mark}  {name.ljust(width)}  {paint(detail, 'dim', enabled=color) if detail else ''}")
        failed += not ok
    total = len(checks)
    print()
    if failed:
        print(paint(f"{failed}/{total} checks FAILED", "red", "bold", enabled=color))
        return 1
    print(paint(f"all {total} checks passed — OCP-SPEC.md §9 conformance",
                "green", "bold", enabled=color))
    return 0


# --- Entry point ------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Host-side OCP client and parser test harness.",
        epilog="--selftest and --replay need no hardware and no pyserial.",
    )
    ap.add_argument("port", nargs="?", help="serial port, e.g. /dev/ttyACM0")
    ap.add_argument("-b", "--baud", type=int, default=ocp.BAUD_DEFAULT)
    ap.add_argument("--replay", type=Path, metavar="FILE",
                    help="parse a canned byte stream instead of a serial port")
    ap.add_argument("--selftest", action="store_true",
                    help="assert the OCP-SPEC.md §9 conformance checklist")
    ap.add_argument("--exec", dest="exec_cmds", action="append", metavar="CMD",
                    help="run a command non-interactively (repeatable)")
    ap.add_argument("--gate", action="store_true",
                    help="drive a live probe through the P1 exit-gate checks")
    ap.add_argument("--gate-wifi", action="store_true",
                    help="P2 Wi-Fi checks against a live probe (prints counts, never SSIDs)")
    ap.add_argument("--gate-sniffer", action="store_true",
                    help="promiscuous-sniffer checks against a live probe (P7, started early; "
                         "table checks SKIP rather than FAIL with no RF traffic nearby)")
    ap.add_argument("--gate-stop", action="store_true",
                    help="scoped-`stop` lane isolation against a live probe (D-16); "
                         "the cross-lane checks SKIP without an SX1262 attached")
    ap.add_argument("--no-color", action="store_true")
    args = ap.parse_args(argv)
    color = not args.no_color and sys.stdout.isatty()

    if args.selftest:
        return mode_selftest(color)
    if args.replay:
        return mode_replay(args.replay, color)
    if args.port and args.gate_wifi:
        return mode_gate_wifi(args.port, args.baud, color)
    if args.port and args.gate_sniffer:
        return mode_gate_sniffer(args.port, args.baud, color)
    if args.port and args.gate_stop:
        return mode_gate_stop(args.port, args.baud, color)
    if args.port and args.gate:
        return mode_gate(args.port, args.baud, color)
    if args.port and args.exec_cmds:
        return mode_exec(args.port, args.baud, args.exec_cmds, color)
    if args.port:
        return mode_serial(args.port, args.baud, color)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
