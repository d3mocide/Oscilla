#!/usr/bin/env python3
"""Mutation test for the fail-closed 802.15.4 capability gate."""
from pathlib import Path
import shutil
import tempfile
import sys

from check_zig_readiness import check


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    if check(ROOT):
        print("FAIL: baseline readiness check failed")
        return 1
    with tempfile.TemporaryDirectory() as temp:
        copy_root = Path(temp)
        shutil.copytree(ROOT / "firmware-c5", copy_root / "firmware-c5")
        path = copy_root / "firmware-c5/main/zig_recon.c"
        code = path.read_text()
        code = code.replace("bool zig_recon_ready(void) { return s_ready; }",
                            "bool zig_recon_ready(void) { return s_lock != NULL; }")
        path.write_text(code)
        if not check(copy_root):
            print("FAIL: readiness mutation was accepted")
            return 1
    print("  zig readiness mutation test: partial-init advertisement rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
