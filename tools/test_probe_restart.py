#!/usr/bin/env python3
"""Verify the deferred probe-restart wiring for the 802.15.4/Wi-Fi/BLE
coexistence defect (github.com/espressif/esp-matter/issues/1851): stopping
the 802.15.4 engine must request a recovery reboot, that reboot must be
held off while LoRa RX is running, and any OCP reply already queued must
reach the deck before the probe actually restarts.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ZIG_SOURCE = ROOT / "firmware-c5/main/zig_recon.c"
DEFAULT_SERVER_SOURCE = ROOT / "firmware-c5/main/ocp_server.c"
DEFAULT_RESTART_SOURCE = ROOT / "firmware-c5/main/probe_restart.c"


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


def case_body(source: str, label: str) -> str | None:
    """Body of a `case LABEL: { ... break; }` block, for switch statements
    (function_body only matches named functions)."""
    match = re.search(re.escape(label) + r"\s*:\s*\{", source)
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
    parser.add_argument("--zig-source", type=Path, default=DEFAULT_ZIG_SOURCE)
    parser.add_argument("--server-source", type=Path, default=DEFAULT_SERVER_SOURCE)
    parser.add_argument("--restart-source", type=Path, default=DEFAULT_RESTART_SOURCE)
    args = parser.parse_args()
    zig_source = args.zig_source.read_text(encoding="utf-8")
    server_source = args.server_source.read_text(encoding="utf-8")
    restart_source = args.restart_source.read_text(encoding="utf-8")

    zig_teardown = function_body(zig_source, "zig_teardown")
    if zig_teardown is None:
        print("FAIL: zig_teardown is missing", file=sys.stderr)
        return 1
    if "probe_restart_request()" not in zig_teardown:
        print("FAIL: zig_teardown does not request a recovery restart", file=sys.stderr)
        return 1
    if "esp_restart()" in zig_teardown:
        print("FAIL: zig_teardown must not call esp_restart() directly — "
              "LoRa RX has to be able to hold it off", file=sys.stderr)
        return 1

    stop_case = case_body(server_source, "case OCP_VID_STOP")
    if stop_case is None:
        print("FAIL: OCP_VID_STOP case is missing", file=sys.stderr)
        return 1
    ack_at = stop_case.find("ocp_emit_compact(OCP_MARK_STOP")
    restart_check_at = stop_case.find("probe_restart_if_safe()")
    if ack_at < 0:
        print("FAIL: STOP handler no longer acks the lane", file=sys.stderr)
        return 1
    if restart_check_at < 0:
        print("FAIL: STOP handler never rechecks a pending probe restart "
              "after LoRa stops", file=sys.stderr)
        return 1
    if restart_check_at < ack_at:
        print("FAIL: STOP handler checks the pending restart before the "
              "[STOP] ack goes out — a fired restart never returns, so the "
              "deck would never see the ack", file=sys.stderr)
        return 1

    restart_if_safe = function_body(restart_source, "probe_restart_if_safe")
    if restart_if_safe is None:
        print("FAIL: probe_restart_if_safe is missing", file=sys.stderr)
        return 1
    guard_at = restart_if_safe.find("lora_radio_is_running()")
    restart_call_at = restart_if_safe.find("esp_restart()")
    if guard_at < 0 or restart_call_at < 0 or guard_at > restart_call_at:
        print("FAIL: probe_restart_if_safe does not gate esp_restart() on "
              "lora_radio_is_running()", file=sys.stderr)
        return 1

    print("probe restart wiring test OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
