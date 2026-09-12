#!/usr/bin/env python3
"""ocp_repl.py — host-side OCP client for the Oscilla probe.

Three modes, in the order you will need them:

    ocp_repl.py --selftest                    # no hardware: assert the spec's
                                              # conformance checklist (§9)
    ocp_repl.py --replay fixtures/x.txt       # no hardware: parse a canned
                                              # byte stream and pretty-print it
    ocp_repl.py /dev/ttyACM0                  # drive a real probe

The point of the serial mode is P1's first exit gate: drive the C5 through the
whole system-verb set from a laptop *before the deck firmware exists*.

Needs pyserial only for the serial mode; --selftest and --replay run on a bare
Python 3.11+.

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
    """Make a decoded field safe to print.

    Field contents are attacker-controlled (an SSID is whatever the AP says it
    is). Once decoded they are real bytes again, so anything that reaches a
    terminal must be re-escaped first — otherwise a crafted SSID injects ANSI
    escapes or newlines into the operator's display. Every renderer, deck
    included, owes the same duty.
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
    """Parse a canned byte stream. Fed in odd-sized chunks so the parser's
    byte-level line assembly is exercised, not just its line handling."""
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

        # The probe may already be mid-boot; give its [HELLO] a moment, then
        # solicit one so we know the caps either way.
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
    checks: list[tuple[str, bool, str]] = []

    def check(name: str, ok: bool, detail: str = "") -> None:
        checks.append((name, bool(ok), detail))

    def items(*lines: str) -> list:
        p = OcpParser()
        out = []
        for ln in lines:
            out.extend(p.feed_line(ln))
        return out

    # ocp.py has not drifted from ocp.h
    drift = ocp.check_against_header()
    check("ocp.py literals match ocp.h", not drift, "; ".join(drift))

    # The command table carries no transmit verb (§7 / D-8)
    tx = [v for v in header_verbs()
          if any(n in v for n in ("_tx", "tx_", "transmit", "send", "inject"))]
    check("no transmit verb in the command table", not tx, ", ".join(tx))

    # 1. Non-marker text is ignored and never advances frame state
    got = items("ets Jul 29 2019", "rst:0x1 (POWERON)", "I (31) boot: ok")
    check("boot chatter parses as noise only",
          len(got) == 3 and all(isinstance(i, Noise) for i in got))

    # 2. An oversized line is discarded without desynchronising the next
    p = OcpParser(max_line_len=64)
    over = b"[SCAN] " + b"A" * 200 + b"\n[STOP] running=0 END\n"
    got = list(p.feed_bytes(over))
    check("oversized line discarded, next line still parses",
          len(got) == 2 and isinstance(got[0], Noise)
          and isinstance(got[1], Frame) and got[1].tag == "[STOP]")

    # 3. [EVT]/[ERR] interleaved inside an open frame are routed out-of-band
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

    # 4. Unknown marker / kind / key are ignored, never fatal
    got = items("[QUANTUM] BEGIN a=1", "[QUANTUM] row", "[QUANTUM] END")
    check("unknown marker parses as an ordinary frame",
          len(got) == 1 and isinstance(got[0], Frame) and got[0].tag == "[QUANTUM]")
    got = items("[EVT] kind=teleport distance=9000")
    check("unknown EVT kind is delivered, not fatal",
          len(got) == 1 and isinstance(got[0], Event) and got[0].kind == "teleport")
    got = items("[STOP] running=0 future_key=7 END")
    check("unknown k=v key is ignored",
          len(got) == 1 and got[0].get("running") == "0")

    # 5. Compact and block forms accepted for any tag
    compact = items("[STOP] running=0 END")
    block = items("[STOP] BEGIN", "[STOP] running=0", "[STOP] END")
    check("compact and block forms both accepted",
          len(compact) == 1 and compact[0].compact
          and len(block) == 1 and not block[0].compact
          and block[0].row_kvs()[0]["running"] == b"0")

    # 6. Escapes round-trip byte-exactly
    payloads = [b"plain", b"", b'quote" and \\ backslash',
                b"newline\nand\rCR", b"caf\xc3\xa9", b"\x00\x01\xff",
                b"comma,separated", "日本語".encode()]
    bad = [p for p in payloads if ocp.decode_field(ocp.encode_field(p)) != p]
    check("escapes round-trip byte-exactly", not bad, repr(bad))

    # An SSID containing a newline cannot break out of its row
    evil = ocp.encode_field(b"evil\nrow")
    check("embedded newline cannot escape a field", "\n" not in evil, repr(evil))

    row = ",".join(ocp.encode_field(f) for f in
                   [b"1", b'a,b"c', b"AA:BB:CC:DD:EE:01", b"6", b"WPA2", b"-50", b"2.4"])
    got = items(f"[SCAN] BEGIN n=1", f"[SCAN] {row}", "[SCAN] END")
    fields = got[0].csv_rows()[0] if got and isinstance(got[0], Frame) else []
    check("CSV row splits on real separators only",
          len(fields) == 7 and fields[1] == b'a,b"c', repr(fields))

    # 7. Unsolicited [HELLO] is delivered even inside an open frame
    got = items("[LORA] BEGIN state=idle",
                "[LORA] freq=0",
                "ets Jul 29 2019",
                "[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps=wifi24,ble END")
    hellos = [i for i in got if isinstance(i, Frame) and i.tag == "[HELLO]"]
    check("HELLO inside an open frame is still delivered",
          len(hellos) == 1 and hellos[0].get("caps") == "wifi24,ble")

    # 8. A frame missing its END is abandoned without wedging the reader
    p = OcpParser()
    list(p.feed_line("[SCAN] BEGIN n=1"))
    abandoned = p.abandon_open_frame()
    after = list(p.feed_line("[STOP] running=0 END"))
    check("abandoning a half-read frame leaves the reader usable",
          abandoned is not None and abandoned.tag == "[SCAN]"
          and len(after) == 1 and after[0].tag == "[STOP]")

    # A re-opened frame does not nest
    got = items("[SCAN] BEGIN n=1", "[SCAN] BEGIN n=2", "[SCAN] END")
    check("a re-opened frame is reported, not nested",
          any(isinstance(i, Noise) for i in got)
          and len([i for i in got if isinstance(i, Frame)]) == 1)

    # 9. pong is recognised
    got = items("pong")
    check("pong is recognised as ping's reply",
          len(got) == 1 and isinstance(got[0], Pong))
    got = items("pong extra")
    check("'pong extra' is not a pong",
          len(got) == 1 and isinstance(got[0], Noise))

    # Malformed input never raises out of the parser
    for junk in ['[SCAN] "unterminated', "[SCAN] BEGIN k=\\xZZ", "[", "[]", "[ ]",
                 "[scan] lower", "\x00\x01\x02", "[SCAN]", "=", "==", "k=", '"'*5]:
        try:
            list(OcpParser().feed_line(junk))
        except Exception as exc:  # noqa: BLE001
            check(f"malformed input {junk!r} does not raise", False, repr(exc))
            break
    else:
        check("malformed input never raises out of the parser", True)

    # Command encoding respects the caps
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

    # The shipped fixture parses end to end
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

    # -- report ------------------------------------------------------------
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
    ap.add_argument("--no-color", action="store_true")
    args = ap.parse_args(argv)
    color = not args.no_color and sys.stdout.isatty()

    if args.selftest:
        return mode_selftest(color)
    if args.replay:
        return mode_replay(args.replay, color)
    if args.port:
        return mode_serial(args.port, args.baud, color)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
