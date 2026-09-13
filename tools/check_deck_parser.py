#!/usr/bin/env python3
"""Diff the deck's C++ parser against the Python reference on a fuzz corpus.

Builds firmware-cardputer/test/host/parser_corpus.cpp with the host compiler,
generates streams with ocp_fuzz.py --emit-corpus, and requires the two
implementations to produce identical item sequences, noise included.

SPDX-License-Identifier: MIT
"""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def build(out: Path) -> Path:
    warn = ["-Wall", "-Wextra", "-Werror"]
    text_o = out / "ocp_text.o"
    subprocess.run(["gcc", "-std=c99", "-O1", *warn, f"-I{ROOT / 'protocol'}", "-c",
                    str(ROOT / "protocol/ocp_text.c"), "-o", str(text_o)], check=True)
    exe = out / "parser_corpus"
    subprocess.run(["g++", "-std=c++17", "-O1", *warn,
                    f"-I{ROOT / 'protocol'}", f"-I{ROOT / 'firmware-cardputer/src'}",
                    str(ROOT / "firmware-cardputer/test/host/parser_corpus.cpp"),
                    str(ROOT / "firmware-cardputer/src/ocp/ocp_parser.cpp"),
                    str(ROOT / "firmware-cardputer/src/ocp/ocp_csv.cpp"),
                    str(text_o), "-o", str(exe)], check=True)
    return exe


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        exe = build(tmp)
        corpus = tmp / "corpus"
        subprocess.run([sys.executable, str(ROOT / "tools/ocp_fuzz.py"), "--emit-corpus",
                        str(corpus), "--corpus-count", str(args.count), "--seed", str(args.seed)],
                       check=True, capture_output=True)

        items = 0
        for stream in sorted(corpus.glob("stream_*.bin")):
            expected = [json.loads(l) for l in stream.with_suffix(".expected.jsonl").read_text().splitlines()]
            res = subprocess.run([str(exe), str(stream)], capture_output=True, text=True)
            if res.returncode != 0:
                print(f"FAIL {stream.name}: harness exit {res.returncode}: {res.stderr.strip()}")
                return 1
            got = [json.loads(l) for l in res.stdout.splitlines()]
            if got != expected:
                i = next((k for k, (a, b) in enumerate(zip(expected, got)) if a != b),
                         min(len(expected), len(got)))
                print(f"FAIL {stream.name}: first difference at item {i} "
                      f"({len(expected)} expected, {len(got)} got)")
                print(f"  py : {expected[i] if i < len(expected) else '<none>'}")
                print(f"  c++: {got[i] if i < len(got) else '<none>'}")
                return 1
            items += len(expected)

        # CSV rows: valid encodings, near-misses, and garbage.
        import random
        sys.path.insert(0, str(ROOT / "tools"))
        import ocp
        rng = random.Random(args.seed)
        rows = []
        for _ in range(3000):
            k = rng.random()
            fields = [bytes(rng.randrange(256) for _ in range(rng.randint(0, 12))) for _ in range(rng.randint(1, 8))]
            row = ",".join(ocp.encode_field(f) for f in fields)
            if k < 0.25:
                row = row.replace(",", rng.choice([" , ", "\t,", ",\x1c", ", "]), rng.randint(0, 3))
            elif k < 0.45:
                cut = rng.randrange(len(row) + 1)
                row = row[:cut] + rng.choice(['"', "\\", "\\q", "\\x+f", ",", "\xa0", ""]) + row[cut:]
            elif k < 0.55:
                row = "".join(chr(rng.randrange(256)) for _ in range(rng.randint(0, 40)))
            rows.append(row.encode("latin-1"))
        csv_file = tmp / "rows.hex"
        csv_file.write_text("\n".join(r.hex() for r in rows) + "\n")
        res = subprocess.run([str(exe), "--csv", str(csv_file)], capture_output=True, text=True, check=True)
        got = [json.loads(l) for l in res.stdout.splitlines()]
        for i, row in enumerate(rows):
            try:
                want = [f.hex() for f in ocp.split_csv_row(row.decode("latin-1"))]
            except ocp.OcpFramingError:
                want = None
            if got[i] != want:
                print(f"FAIL csv row {i}: {row!r}\n  py : {want}\n  c++: {got[i]}")
                return 1
        bad = sum(g is None for g in got)

    print(f"  deck parser matches ocp.py: {args.count} streams, {items} items, byte-wise chunking identical")
    print(f"  deck CSV splitter matches ocp.py: {len(rows)} rows ({bad} malformed, rejected identically)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
