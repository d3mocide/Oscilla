#!/usr/bin/env python3
"""ocp_fuzz.py — property fuzzer for the OCP reference parser.

Properties (each run against thousands of generated streams):
  raises     no input makes the parser raise
  chunks     results are identical however the bytes are split
  noise      valid items interleaved with boot text/log noise come out exact
  recovery   after any garbage, [HELLO] or a timeout brings the reader back
  bounded    line buffer and open-frame rows never exceed the caps
  escapes    encode/decode round-trips arbitrary bytes

  ocp_fuzz.py                        run all properties
  ocp_fuzz.py --emit-corpus DIR      write streams + reference output, for
                                     diffing another parser implementation

SPDX-License-Identifier: MIT
"""

from __future__ import annotations

import argparse
import json
import random
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ocp  # noqa: E402
from ocp import Error, Event, Frame, Noise, OcpParser, Pong  # noqa: E402

# Captured from the C5 over Grove (docs/hardware/link-bringup.md) plus IDF logs.
BOOT_NOISE = [
    "ESP-ROM:esp32c5-eco2-20250121", "Build:Jan 21 2025",
    "rst:0xc (SW_CPU),boot:0x18 (SPI_FAST_FLASH_BOOT)", "Core0 Saved PC:0x408069ee",
    "SPI mode:DIO, clock div:1", "load:0x408556c0,len:0x1714", "entry 0x4084bbaa",
    "I (31) boot: ESP-IDF v5.5.1 2nd stage bootloader", "W (4821) wifi: exceed max band",
    "E (120) task_wdt: Task watchdog got triggered.", "Guru Meditation Error: Core  0 panic'ed",
]
TAGS = ["[SCAN]", "[INSPECT]", "[STATUS]", "[LORA]", "[ZIG]", "[BLE]", "[CFG]"]
KEYS = ["n", "ch", "rssi", "owner", "state", "idx", "mfp_capable", "uptime_ms"]


# --- Canonical form, shared with other implementations via --emit-corpus -----

def canon(item) -> dict:
    hexkv = lambda kv: {k: v.hex() for k, v in sorted(kv.items())}  # noqa: E731
    if isinstance(item, Frame):
        return {"t": "frame", "tag": item.tag, "compact": item.compact,
                "kv": hexkv(item.kv), "rows": [r.encode("latin-1").hex() for r in item.rows]}
    if isinstance(item, Event):
        return {"t": "evt", "kv": hexkv(item.kv)}       # kind lives in kv
    if isinstance(item, Error):
        return {"t": "err", "kv": hexkv(item.kv)}       # code and msg live in kv
    if isinstance(item, Pong):
        return {"t": "pong"}
    return {"t": "noise"}


def parse(data: bytes, chunks: list[int] | None = None) -> list[dict]:
    p = OcpParser()
    out, i = [], 0
    for size in chunks or [len(data)]:
        out.extend(canon(x) for x in p.feed_bytes(data[i:i + size]))
        i += size
    out.extend(canon(x) for x in p.feed_bytes(data[i:]))
    return out


# --- Generators --------------------------------------------------------------

def rand_bytes(rng: random.Random, lo: int = 0, hi: int = 24) -> bytes:
    return bytes(rng.randrange(256) for _ in range(rng.randint(lo, hi)))


def kv_tokens(rng: random.Random, n: int) -> tuple[list[str], dict]:
    toks, kv = [], {}
    for key in rng.sample(KEYS, n):
        raw = rand_bytes(rng, 1, 12) if rng.random() < 0.5 else str(rng.randint(-99, 9999)).encode()
        toks.append(f"{key}={ocp.encode_value(raw)}")
        kv[key] = raw
    return toks, kv


def gen_item(rng: random.Random) -> tuple[list[str], list[dict]]:
    """One valid protocol unit: its wire lines and the items it must yield."""
    kind = rng.choice(["block", "block", "compact", "evt", "err", "pong"])
    if kind == "pong":
        return ["pong"], [{"t": "pong"}]
    if kind == "evt":
        toks, kv = kv_tokens(rng, rng.randint(0, 3))
        kv["kind"] = b"sniff"
        return [" ".join(["[EVT]", "kind=sniff", *toks])], [canon(Event("sniff", kv))]
    if kind == "err":
        msg = rand_bytes(rng, 0, 20)
        line = f"[ERR] code=busy msg={ocp.encode_field(msg)}"
        kv = {"code": b"busy", "msg": msg}
        return [line], [canon(Error("busy", "", kv))]
    tag = rng.choice(TAGS)
    toks, kv = kv_tokens(rng, rng.randint(1, 3))
    if kind == "compact":
        return [" ".join([tag, *toks, "END"])], [canon(Frame(tag, kv, [], True))]
    rows = [",".join(ocp.encode_field(rand_bytes(rng, 0, 16)) for _ in range(rng.randint(1, 5)))
            for _ in range(rng.randint(0, 6))]
    lines = [" ".join([tag, "BEGIN", *toks]), *(f"{tag} {r}" for r in rows), f"{tag} END"]
    return lines, [canon(Frame(tag, kv, rows, False))]


def gen_noise_line(rng: random.Random) -> str:
    """A non-protocol line: must never form, disturb or close a frame."""
    if rng.random() < 0.6:
        return rng.choice(BOOT_NOISE)
    text = "".join(chr(rng.randrange(0x20, 0x7F)) for _ in range(rng.randint(1, 60)))
    return "x" + text.lstrip("[")          # never marker-shaped, never 'pong'


def gen_garbage(rng: random.Random) -> bytes:
    parts = []
    for _ in range(rng.randint(1, 12)):
        r = rng.random()
        if r < 0.25:
            parts.append(rand_bytes(rng, 0, 80))
        elif r < 0.45:
            parts.append(f"{rng.choice(TAGS)} BEGIN".encode())
        elif r < 0.55:
            parts.append(f"{rng.choice(TAGS)} END".encode())
        elif r < 0.65:
            parts.append(b'[SCAN] "unterminated')
        elif r < 0.75:
            parts.append(b"A" * rng.randint(400, 1400))   # over-long
        elif r < 0.80:
            parts.append(rng.choice(BOOT_NOISE).encode())
        elif r < 0.88:
            # k=v shapes with broken escapes, on every line type that parses kv
            bad = rng.choice([r'"\q"', r'"\x4"', r'"\xZZ"', r'"\"', r'"ok\"', '"\\x"'])
            head = rng.choice([f"{rng.choice(TAGS)} BEGIN", "[EVT] kind=x", "[ERR] code=busy",
                               "[HELLO]", rng.choice(TAGS)])
            tail = " END" if "BEGIN" not in head and rng.random() < 0.6 else ""
            parts.append(f"{head} {rng.choice(KEYS)}={bad}{tail}".encode())
        else:
            parts.append(rng.choice([b"\r", b"\n", b"\r\n", b"\x00", b"\\", b'"', b"END",
                                     b"\x0c", b"\x1c", b"\x85", b"\xa0", b" \t "]))
        parts.append(rng.choice([b"\n", b"\r\n", b"", b" "]))
    return b"".join(parts)


def random_chunks(rng: random.Random, n: int) -> list[int]:
    sizes, left = [], n
    while left > 0:
        s = rng.choice([1, 1, 2, 3, 7, 64, 513])
        sizes.append(min(s, left))
        left -= s
    return sizes


# --- Properties ----------------------------------------------------------------

def prop_raises_and_bounded(rng: random.Random) -> str | None:
    data = gen_garbage(rng)
    p = OcpParser()
    try:
        for size in random_chunks(rng, len(data)):
            list(p.feed_bytes(data[:size]))
            data = data[size:]
            # White-box: the caps the deck's RAM budget depends on.
            if len(p._buf) > p.max_line_len:
                return f"line buffer {len(p._buf)} > {p.max_line_len}"
            if p._open is not None and len(p._open.rows) > p.max_frame_rows:
                return f"open frame rows {len(p._open.rows)} > {p.max_frame_rows}"
    except Exception as exc:  # noqa: BLE001
        return f"raised {type(exc).__name__}: {exc}"
    return None


def prop_chunks(rng: random.Random) -> str | None:
    data = gen_garbage(rng) + b"\n" + "\n".join(gen_item(rng)[0]).encode() + b"\n"
    whole = parse(data)
    split = parse(data, random_chunks(rng, len(data)))
    return None if whole == split else "chunked parse differs from whole parse"


def prop_noise(rng: random.Random) -> str | None:
    lines, expected = [], []
    for _ in range(rng.randint(1, 8)):
        item_lines, items = gen_item(rng)
        for i, ln in enumerate(item_lines):
            lines.append(ln)
            # Noise may land anywhere, including between a frame's rows.
            if i < len(item_lines) - 1 and rng.random() < 0.3:
                lines.append(gen_noise_line(rng))
        expected.extend(items)
        if rng.random() < 0.5:
            lines.append(gen_noise_line(rng))
    eol = rng.choice(["\n", "\r\n"])
    data = eol.join(lines).encode() + eol.encode()
    got = [x for x in parse(data, random_chunks(rng, len(data))) if x["t"] != "noise"]
    if got != expected:
        return f"expected {len(expected)} items, got {len(got)}"
    return None


def prop_recovery(rng: random.Random) -> str | None:
    garbage = gen_garbage(rng)
    hello = b"\n[HELLO] proto=1 fw=oscilla-c5 ver=0.1.0 caps= END\n[STOP] running=0 END\n"
    got = parse(garbage + hello)
    tail = [x for x in got if x["t"] == "frame"][-2:]
    if [f["tag"] for f in tail] != ["[HELLO]", "[STOP]"]:
        return f"no recovery via [HELLO]: last frames {[f['tag'] for f in tail]}"

    p = OcpParser()
    list(p.feed_bytes(garbage))
    p.abandon_open_frame()                     # what a deck does on timeout
    after = [canon(x) for x in p.feed_bytes(b"[STOP] running=0 END\n")]
    if not any(x["t"] == "frame" and x["tag"] == "[STOP]" for x in after):
        return "no recovery via timeout abandon"
    return None


def prop_escapes(rng: random.Random) -> str | None:
    raw = rand_bytes(rng, 0, 64)
    if ocp.decode_field(ocp.encode_field(raw)) != raw:
        return f"round-trip lost bytes: {raw!r}"
    junk = "".join(chr(rng.randrange(0x20, 0x7F)) for _ in range(rng.randint(0, 20)))
    try:
        ocp.decode_field(junk)
    except ocp.OcpFramingError:
        pass
    except Exception as exc:  # noqa: BLE001
        return f"decode raised {type(exc).__name__} on {junk!r}"
    return None


PROPERTIES = {
    "raises+bounded": prop_raises_and_bounded,
    "chunks": prop_chunks,
    "noise": prop_noise,
    "recovery": prop_recovery,
    "escapes": prop_escapes,
}


def run(iterations: int, seed: int) -> int:
    failed = 0
    for name, prop in PROPERTIES.items():
        for i in range(iterations):
            case_seed = seed * 1_000_003 + i
            try:
                msg = prop(random.Random(case_seed))
            except Exception as exc:  # noqa: BLE001  a raise is a failure, not a crash
                msg = f"raised {type(exc).__name__}: {exc}"

            if msg:
                print(f"  FAIL  {name:15} case seed {case_seed}: {msg}")
                print(f"        reproduce: ocp_fuzz.py --property {name} --case {case_seed}")
                failed += 1
                break
        else:
            print(f"  PASS  {name:15} {iterations} cases")
    print()
    print("fuzz: all properties hold" if not failed else f"fuzz: {failed} properties FAILED")
    return 1 if failed else 0


def emit_corpus(out: Path, count: int, seed: int) -> int:
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(seed)
    for n in range(count):
        lines = []
        for _ in range(rng.randint(1, 10)):
            if rng.random() < 0.3:
                lines.append(gen_garbage(rng))
            else:
                lines.append(("\n".join(gen_item(rng)[0]) + "\n").encode())
        data = b"\n".join(lines)
        (out / f"stream_{n:04d}.bin").write_bytes(data)
        with open(out / f"stream_{n:04d}.expected.jsonl", "w") as f:
            for item in parse(data):
                f.write(json.dumps(item, sort_keys=True) + "\n")
    print(f"wrote {count} streams + reference output to {out}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--iterations", type=int, default=2000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--property", choices=PROPERTIES)
    ap.add_argument("--case", type=int, help="replay one case seed")
    ap.add_argument("--emit-corpus", type=Path, metavar="DIR")
    ap.add_argument("--corpus-count", type=int, default=200)
    args = ap.parse_args()

    if args.emit_corpus:
        return emit_corpus(args.emit_corpus, args.corpus_count, args.seed)
    if args.property and args.case is not None:
        msg = PROPERTIES[args.property](random.Random(args.case))
        print(msg or "case passes")
        return 1 if msg else 0
    return run(args.iterations, args.seed)


if __name__ == "__main__":
    sys.exit(main())
