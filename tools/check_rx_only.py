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


def sources(root, dirs):
    for d in dirs:
        base = root / d
        if base.exists():
            yield from (p for p in sorted(base.rglob("*")) if p.suffix in EXTS and p.is_file())


def check(root, dirs, finder, include_re=None) -> list[str]:
    problems = []
    for path in sources(root, dirs):
        raw = path.read_text(errors="replace")
        rel = path.relative_to(root)
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


LORA_RX_WRITE_OPCODES = {
    "OP_SET_SLEEP", "OP_SET_STANDBY", "OP_SET_RX", "OP_SET_REGULATOR_MODE",
    "OP_CALIBRATE", "OP_SET_DIO3_TCXO_CTRL", "OP_SET_DIO2_RF_SWITCH",
    "OP_SET_DIO_IRQ_PARAMS", "OP_CLEAR_IRQ_STATUS", "OP_SET_RF_FREQUENCY",
    "OP_SET_PACKET_TYPE", "OP_SET_MODULATION_PARAMS", "OP_SET_PACKET_PARAMS",
    "OP_SET_BUFFER_BASE_ADDR", "OP_CLEAR_DEVICE_ERRORS",
}
LORA_RX_READ_OPCODES = {
    "OP_GET_IRQ_STATUS", "OP_GET_RX_BUFFER_STATUS", "OP_GET_PACKET_STATUS",
    "OP_GET_STATUS", "OP_GET_DEVICE_ERRORS",
}
LORA_FORBIDDEN_OPCODE_VALUES = {"0x83", "0x8e", "0x95", "0xd1", "0xd2"}
# SX1262 registers the driver may write, name -> address. PA/TX-clamp/OCP
# registers must never appear here (D-8).
LORA_RX_WRITE_REGISTERS = {"REG_LORA_SYNC_WORD_MSB": "0x0740"}


def configuration_problems(root: Path) -> list[str]:
    problems = []

    wifi_path = root / "firmware-c5/main/wifi_recon.c"
    if not wifi_path.exists():
        problems.append("firmware-c5/main/wifi_recon.c: missing passive Wi-Fi scan wrapper")
    else:
        code = strip_comments_and_strings(wifi_path.read_text(errors="replace"))
        if not re.search(r"static\s+wifi_scan_config_t\s+wifi_passive_scan_config\s*\(\s*void\s*\)", code):
            problems.append("firmware-c5/main/wifi_recon.c: passive scan wrapper is missing")
        if not re.search(r"\.scan_type\s*=\s*WIFI_SCAN_TYPE_PASSIVE\b", code):
            problems.append("firmware-c5/main/wifi_recon.c: passive scan type is not explicitly selected")
        if len(re.findall(r"\besp_wifi_scan_start\s*\(", code)) != 1:
            problems.append("firmware-c5/main/wifi_recon.c: scan start must have exactly one audited call")
        if not re.search(r"wifi_scan_config_t\s+cfg\s*=\s*wifi_passive_scan_config\s*\(\s*\)", code):
            problems.append("firmware-c5/main/wifi_recon.c: scan does not use the passive wrapper")

    ble_path = root / "firmware-c5/main/ble_recon.c"
    if not ble_path.exists():
        problems.append("firmware-c5/main/ble_recon.c: missing passive BLE scan wrapper")
    else:
        code = strip_comments_and_strings(ble_path.read_text(errors="replace"))
        if not re.search(r"static\s+struct\s+ble_gap_disc_params\s+ble_passive_disc_params\s*\(\s*void\s*\)", code):
            problems.append("firmware-c5/main/ble_recon.c: passive discovery wrapper is missing")
        if not re.search(r"params\.passive\s*=\s*1\b", code):
            problems.append("firmware-c5/main/ble_recon.c: BLE discovery is not explicitly passive")
        if len(re.findall(r"\bble_gap_disc\s*\(", code)) != 1:
            problems.append("firmware-c5/main/ble_recon.c: discovery start must have exactly one audited call")
        if not re.search(r"struct\s+ble_gap_disc_params\s+params\s*=\s*ble_passive_disc_params\s*\(\s*\)", code):
            problems.append("firmware-c5/main/ble_recon.c: discovery does not use the passive wrapper")

    lora_path = root / "firmware-c5/main/lora_radio.c"
    if not lora_path.exists():
        problems.append("firmware-c5/main/lora_radio.c: missing SX1262 RX-only driver")
    else:
        code = strip_comments_and_strings(lora_path.read_text(errors="replace"))
        for value in LORA_FORBIDDEN_OPCODE_VALUES:
            if re.search(rf"\b{re.escape(value)}\b", code, re.IGNORECASE):
                problems.append(f"firmware-c5/main/lora_radio.c: forbidden transmit opcode {value}")
        for match in re.finditer(r"\bcmd_write\s*\(([^)]*)\)", code, re.DOTALL):
            first = match.group(1).split(",", 1)[0].strip()
            if first.startswith("uint8_t"):
                continue
            if first not in LORA_RX_WRITE_OPCODES:
                problems.append(f"firmware-c5/main/lora_radio.c: cmd_write opcode is not RX-allowlisted: {first}")
        for match in re.finditer(r"\bcmd_read\s*\(([^)]*)\)", code, re.DOTALL):
            first = match.group(1).split(",", 1)[0].strip()
            if first.startswith("uint8_t"):
                continue
            if first not in LORA_RX_READ_OPCODES:
                problems.append(f"firmware-c5/main/lora_radio.c: cmd_read opcode is not RX-allowlisted: {first}")
        for match in re.finditer(r"\bwrite_register\s*\(([^)]*)\)", code, re.DOTALL):
            first = match.group(1).split(",", 1)[0].strip()
            if first.startswith("uint16_t"):
                continue
            if first not in LORA_RX_WRITE_REGISTERS:
                problems.append(f"firmware-c5/main/lora_radio.c: write_register address is not RX-allowlisted: {first}")
        for name, value in re.findall(r"#define\s+(REG_\w+)\s+(0x[0-9A-Fa-f]+)", code):
            if LORA_RX_WRITE_REGISTERS.get(name, "").lower() != value.lower():
                problems.append(f"firmware-c5/main/lora_radio.c: register {name}={value} is not RX-allowlisted")

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


def main(root: Path = ROOT) -> int:
    problems = check(root, PROBE_DIRS, probe_finder) + check(root, DECK_DIRS, deck_finder, DECK_BANNED_INCLUDE)
    problems += configuration_problems(root)
    # ESP-IDF changes auto-ACK state with promiscuous mode. D-17's 802.15.4
    # adapter must enable it before RX, and must never turn it back off while
    # the radio is enabled (that would re-enable automatic ACK transmission).
    zig_radio = root / "firmware-c5/main/zig_radio.c"
    if not zig_radio.exists():
        problems.append("firmware-c5/main/zig_radio.c: missing D-17 802.15.4 receive-only adapter")
    else:
        code = strip_comments_and_strings(zig_radio.read_text(errors="replace"))
        if not re.search(r"\besp_ieee802154_set_promiscuous\s*\(\s*true\s*\)", code):
            problems.append("firmware-c5/main/zig_radio.c: missing promiscuous=true required to disable 802.15.4 auto-ACK TX")
        if re.search(r"\besp_ieee802154_set_promiscuous\s*\(\s*false\s*\)", code):
            problems.append("firmware-c5/main/zig_radio.c: promiscuous=false can re-enable 802.15.4 auto-ACK TX")
    if problems:
        print("FAIL: transmit-capable API reachable from firmware source (D-8):")
        print("\n".join(f"  {p}" for p in problems))
        return 1
    n = sum(1 for _ in sources(root, PROBE_DIRS)) + sum(1 for _ in sources(root, DECK_DIRS))
    print(f"  receive-only: {n} source files, no transmit-capable API ({len(PROBE_BANNED)} banned on probe, all radio APIs banned on deck)")
    return 0


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1]) if len(sys.argv) == 2 else ROOT))
