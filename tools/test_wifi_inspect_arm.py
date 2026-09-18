#!/usr/bin/env python3
"""Ensure every AP inspection owns its promiscuous RX configuration."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "firmware-c5/main/wifi_inspect.c"


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
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")

    arm = function_body(source, "arm_capture")
    command = function_body(source, "wifi_cmd_inspect")
    if arm is None or command is None:
        print("FAIL: inspection does not have an arm_capture path", file=sys.stderr)
        return 1

    required = (
        "WIFI_PROMIS_FILTER_MASK_MGMT",
        "esp_wifi_set_promiscuous_filter(&filter)",
        "esp_wifi_set_promiscuous_rx_cb(on_frame)",
    )
    if any(call not in arm for call in required):
        print("FAIL: inspection arm path does not reinstall management RX configuration", file=sys.stderr)
        return 1

    arm_at = command.find("arm_capture()")
    enable_at = command.find("esp_wifi_set_promiscuous(true)")
    timer_at = command.find("esp_timer_start_once(s_timer")
    if arm_at < 0 or enable_at < 0 or timer_at < 0 or not (arm_at < enable_at < timer_at):
        print("FAIL: inspection does not arm its RX callback before capture starts", file=sys.stderr)
        return 1

    print("wifi inspect arm test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
