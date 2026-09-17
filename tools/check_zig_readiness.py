#!/usr/bin/env python3
"""Check that 802.15.4 capability readiness is fail-closed."""
from pathlib import Path
import re
import sys


def check(root: Path) -> list[str]:
    path = root / "firmware-c5/main/zig_recon.c"
    if not path.exists():
        return ["zig_recon.c is missing"]
    code = path.read_text(errors="replace")
    problems = []
    if not re.search(r"static\s+bool\s+s_ready\s*;", code):
        problems.append("dedicated s_ready flag is missing")
    if code.count("s_ready = false") != 1:
        problems.append("init must clear s_ready before attempting dependencies")
    if code.count("s_ready = true") != 1 or not re.search(r"if\s*\(err\s*==\s*ESP_OK\)\s*s_ready\s*=\s*true", code):
        problems.append("s_ready must be set only after all init steps succeed")
    if not re.search(r"bool\s+zig_recon_ready\s*\(void\)\s*\{\s*return\s+s_ready\s*;\s*\}", code):
        problems.append("zig_recon_ready must return s_ready, not partial resource state")
    if "return s_lock != NULL" in code:
        problems.append("partial mutex allocation must not advertise readiness")
    return problems


def main(root: Path) -> int:
    problems = check(root)
    if problems:
        print("FAIL: 802.15.4 readiness is not fail-closed:")
        print("\n".join(f"  {p}" for p in problems))
        return 1
    print("  zig readiness: dedicated post-init flag verified")
    return 0


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]))
