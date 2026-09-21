#!/usr/bin/env python3
"""Verify packet-monitor teardown restores the Wi-Fi scan baseline."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "firmware-c5/main/wifi_spectrum.c"
DEFAULT_RECON_SOURCE = ROOT / "firmware-c5/main/wifi_recon.c"


def function_body(source: str, name: str) -> str | None:
    match = re.search(r"(?:static\s+)?[\w\s*]+\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{", source)
    if not match:
        return None
    depth = 1
    pos = match.end()
    while pos < len(source) and depth:
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
        pos += 1
    return source[match.end():pos - 1] if depth == 0 else None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--recon-source", type=Path, default=DEFAULT_RECON_SOURCE)
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")
    recon_source = args.recon_source.read_text(encoding="utf-8")
    cleanup = function_body(source, "spectrum_wifi_teardown")
    if cleanup is None:
        print("FAIL: spectrum_wifi_teardown is missing", file=sys.stderr)
        return 1

    required = (
        "esp_wifi_set_promiscuous(false)",
        "esp_wifi_set_promiscuous_rx_cb(NULL)",
        "wifi_recon_restore_shared_phy()",
    )
    missing = [call for call in required if call not in cleanup]
    release_at = cleanup.find("arbiter_release(PHY_OWNER_WIFI)")
    if missing or release_at < 0 or any(cleanup.find(call) > release_at for call in required):
        print("FAIL: spectrum teardown does not reset Wi-Fi before release", file=sys.stderr)
        return 1

    recovery = function_body(recon_source, "wifi_recon_restore_shared_phy")
    recovery_required = (
        "esp_wifi_stop()",
        "esp_wifi_start()",
        "esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO)",
        "esp_wifi_set_ps(WIFI_PS_NONE)",
    )
    if recovery is None or any(call not in recovery for call in recovery_required):
        print("FAIL: Wi-Fi recovery does not rebuild the passive STA baseline", file=sys.stderr)
        return 1

    for name in ("cv_teardown", "pm_teardown"):
        body = function_body(source, name)
        if body is None or "spectrum_wifi_teardown();" not in body:
            print(f"FAIL: {name} bypasses shared Wi-Fi teardown", file=sys.stderr)
            return 1

    print("wifi spectrum teardown test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
