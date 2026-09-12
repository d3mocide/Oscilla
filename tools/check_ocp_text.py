#!/usr/bin/env python3
"""Diff the C field encoder against the Python reference.

protocol/test_ocp_text.c prints `<input hex> <escaped>` for a corpus; this
asserts tools/ocp.py produces the identical escape and decodes it back to the
original bytes. Both firmwares and the deck depend on these agreeing.

SPDX-License-Identifier: MIT
"""

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ocp  # noqa: E402


def main(corpus_bin: str) -> int:
    out = subprocess.run([corpus_bin], capture_output=True, text=True, check=True).stdout
    lines = [l for l in out.splitlines() if l.strip()]
    if not lines:
        print("no corpus produced", file=sys.stderr)
        return 1

    mismatches = []
    for lineno, line in enumerate(lines, 1):
        hexin, _, c_escaped = line.partition(" ")
        raw = bytes.fromhex(hexin)

        py_escaped = ocp.encode_field(raw)
        if py_escaped != c_escaped:
            mismatches.append(
                f"  line {lineno}: input {raw!r}\n"
                f"    C : {c_escaped}\n"
                f"    py: {py_escaped}"
            )
            continue

        back = ocp.decode_field(c_escaped)
        if back != raw:
            mismatches.append(
                f"  line {lineno}: round-trip lost bytes\n"
                f"    in : {raw!r}\n"
                f"    out: {back!r}"
            )

    if mismatches:
        print(f"FAIL: {len(mismatches)} of {len(lines)} corpus entries disagree",
              file=sys.stderr)
        print("\n".join(mismatches[:10]), file=sys.stderr)
        return 1

    print(f"  ocp_text.c matches ocp.py on {len(lines)} payloads")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
