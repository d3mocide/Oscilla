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

    def check(name: str, ok: bool, detail: str = "") -> None:
        checks.append((name, bool(ok), detail))

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

        err = first(p.command("scan_networks"), Error)
        check("radio verb with no caps -> code=nocap",
              err is not None and err.code == "nocap", err.code if err else "")

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

    width = max(len(n) for n, _, _ in checks)
    failed = 0
    for name, ok, detail in checks:
        mark = (paint("PASS", "green", enabled=color) if ok
                else paint("FAIL", "red", "bold", enabled=color))
        print(f"  {mark}  {name.ljust(width)}  "
              f"{paint(detail, 'dim', enabled=color) if detail else ''}")
        failed += not ok
    print()
    if failed:
        print(paint(f"{failed}/{len(checks)} checks FAILED", "red", "bold", enabled=color))
        return 1
    print(paint(f"all {len(checks)} checks passed — P1 probe exit gate",
                "green", "bold", enabled=color))
    return 0


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
    ap.add_argument("--no-color", action="store_true")
    args = ap.parse_args(argv)
    color = not args.no_color and sys.stdout.isatty()

    if args.selftest:
        return mode_selftest(color)
    if args.replay:
        return mode_replay(args.replay, color)
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
