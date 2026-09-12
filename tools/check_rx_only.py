#!/usr/bin/env python3
"""check_rx_only.py — fail if firmware source can reach a transmit API.

The verb table guarantees no transmit *verb* (D-8); this guarantees no verb's
handler calls a transmit-capable driver API instead. Comments and string
literals are stripped first, so documenting a banned call is fine.

  probe  may use radios, receive-only: listed transmit APIs are banned
  deck   radios stay off in v1 (DESIGN §3): any radio API is banned

SPDX-License-Identifier: MIT
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Identifiers that transmit, or configure a mode that does.
PROBE_BANNED = {
    # Wi-Fi
    "WIFI_SCAN_TYPE_ACTIVE": "active scan transmits probe requests (OCP-SPEC §10.1)",
    "esp_wifi_80211_tx": "raw 802.11 transmit",
    "esp_wifi_connect": "association transmits",
    "WIFI_MODE_AP": "soft-AP beacons",
    "WIFI_MODE_APSTA": "soft-AP beacons",
    "WIFI_MODE_NAN": "NAN transmits",
    "esp_wifi_action_tx_req": "action frame transmit",
    "esp_wifi_wps_start": "WPS transmits",
    "esp_wifi_ftm_initiate_session": "FTM transmits",
    "esp_now_send": "ESP-NOW transmit",
    "esp_now_init": "ESP-NOW",
    "esp_supp_dpp_start_listening": "DPP exchanges transmit",
    # BLE (NimBLE and Bluedroid)
    "ble_gap_adv_start": "BLE advertising",
    "ble_gap_ext_adv_start": "BLE advertising",
    "ble_gap_connect": "BLE connection",
    "esp_ble_gap_start_advertising": "BLE advertising",
    "esp_ble_gattc_open": "BLE connection",
    # 802.15.4
    "esp_ieee802154_transmit": "802.15.4 transmit",
    "esp_ieee802154_transmit_at": "802.15.4 transmit",
    # SX1262 (our driver's names, reserved so they can't appear)
    "SX126X_CMD_SET_TX": "LoRa transmit",
    "sx1262_transmit": "LoRa transmit",
    "lora_tx": "LoRa transmit",
}

# Includes are matched with strings kept: `#include "esp_wifi.h"` is a literal.
DECK_BANNED_INCLUDE = re.compile(
    r"#\s*include\s*[<\"](WiFi|esp_wifi|BLEDevice|NimBLEDevice|esp_now|esp_bt)\w*\.h[>\"]")

DECK_BANNED_PATTERNS = [
    (r"\besp_wifi_\w+", "deck Wi-Fi stays off in v1"),
    (r"\bWiFi\s*\.", "deck Wi-Fi stays off in v1"),
    (r"\besp_now_\w+", "deck radios stay off in v1"),
    (r"\bble_\w+|\besp_ble_\w+|\bBLEDevice\b|\bNimBLE\w*", "deck BLE stays off in v1"),
]

PROBE_DIRS = ["firmware-c5/main", "firmware-c5/components", "protocol"]
DECK_DIRS = ["firmware-cardputer/src", "firmware-cardputer/bench"]
EXTS = {".c", ".h", ".cpp", ".hpp", ".cc", ".inc"}


def strip_comments_and_strings(src: str, keep_strings: bool = False) -> str:
    """Blank out comments (and literals unless keep_strings), keeping line numbers."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j
        elif c in "\"'" and not keep_strings:
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(" " * (j - i)); i = j
        else:
            out.append(c); i += 1
    return "".join(out)


def sources(dirs):
    for d in dirs:
        base = ROOT / d
        if base.exists():
            yield from (p for p in sorted(base.rglob("*")) if p.suffix in EXTS and p.is_file())


def check(dirs, finder, include_re=None) -> list[str]:
    problems = []
    for path in sources(dirs):
        raw = path.read_text(errors="replace")
        rel = path.relative_to(ROOT)
        code = strip_comments_and_strings(raw)
        for lineno, line in enumerate(code.splitlines(), 1):
            for why, token in finder(line):
                problems.append(f"{rel}:{lineno}: {token} — {why}")
        if include_re:
            for lineno, line in enumerate(strip_comments_and_strings(raw, keep_strings=True).splitlines(), 1):
                m = include_re.search(line)
                if m:
                    problems.append(f"{rel}:{lineno}: {m.group(0)} — deck radios stay off in v1")
    return problems


def probe_finder(line):
    for token, why in PROBE_BANNED.items():
        if re.search(rf"\b{re.escape(token)}\b", line):
            yield why, token


def deck_finder(line):
    for pattern, why in DECK_BANNED_PATTERNS:
        m = re.search(pattern, line)
        if m:
            yield why, m.group(0)


def main() -> int:
    problems = check(PROBE_DIRS, probe_finder) + check(DECK_DIRS, deck_finder, DECK_BANNED_INCLUDE)
    if problems:
        print("FAIL: transmit-capable API reachable from firmware source (D-8):")
        print("\n".join(f"  {p}" for p in problems))
        return 1
    n = sum(1 for _ in sources(PROBE_DIRS)) + sum(1 for _ in sources(DECK_DIRS))
    print(f"  receive-only: {n} source files, no transmit-capable API ({len(PROBE_BANNED)} banned on probe, all radio APIs banned on deck)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
