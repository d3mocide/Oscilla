#!/usr/bin/env python3
"""Build compact sorted binary OUI (/oscilla/oui.bin) and Bluetooth SIG
Company-ID (/oscilla/bt_mfr.bin) lookup tables for Oscilla (WP-12a).

Stdlib-only Python 3. Never bundles third-party registries in the repo; run
against your own downloaded IEEE oui.csv / Bluetooth SIG company_identifiers.
"""
import argparse
import csv
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

HEADER_SIZE = 16
RECORD_SIZE = 24
OUI_KEY_SIZE = 3
BT_KEY_SIZE = 2


def sanitize_ascii(text: str, max_len: int) -> bytes:
    cleaned = "".join(c if 0x20 <= ord(c) <= 0x7E else " " for c in text)
    cleaned = " ".join(cleaned.split()).strip()
    raw = cleaned.encode("ascii", "ignore")[:max_len]
    return raw.ljust(max_len, b"\x00")


def parse_oui_csv(path: Path) -> dict[int, str]:
    entries: dict[int, str] = {}
    with path.open("r", encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or len(row) < 2:
                continue
            # Support both IEEE MA-L CSV (Registry,Assignment,Org,...) and simple (AABBCC,Org)
            if len(row) >= 3 and row[0].strip().upper() in ("MA-L", "OUI"):
                hex_str, org = row[1].strip(), row[2].strip()
            else:
                hex_str, org = row[0].strip(), row[1].strip()
            hex_clean = re.sub(r"[:-]", "", hex_str)
            if not re.fullmatch(r"[0-9A-Fa-f]{6}", hex_clean):
                continue
            key = int(hex_clean, 16)
            if org and key not in entries:
                entries[key] = org
    return entries


def parse_bt_file(path: Path) -> dict[int, str]:
    entries: dict[int, str] = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    # Bluetooth SIG YAML (- value: 0x004C / name: Apple, Inc.)
    cur_val = None
    for line in text.splitlines():
        m_val = re.search(r"\bvalue:\s*(0x[0-9A-Fa-f]+|\d+)", line)
        if m_val:
            cur_val = int(m_val.group(1), 0)
            continue
        m_name = re.search(r"\bname:\s*['\"]?(.+?)['\"]?\s*$", line)
        if m_name and cur_val is not None:
            if 0 <= cur_val <= 0xFFFF and cur_val not in entries:
                entries[cur_val] = m_name.group(1).strip()
            cur_val = None
    if entries:
        return entries

    # CSV fallback: Hex/Decimal ID, Company Name
    with path.open("r", encoding="utf-8", errors="replace", newline="") as f:
        for row in csv.reader(f):
            if len(row) < 2:
                continue
            tok = row[0].strip()
            if not re.fullmatch(r"(0[xX][0-9A-Fa-f]{1,4}|[0-9A-Fa-f]{4}|\d{1,5})", tok):
                continue
            try:
                val = int(tok, 16) if (tok.lower().startswith("0x") or len(tok) == 4 and any(c in "abcdefABCDEF" for c in tok)) else int(tok, 0)
            except ValueError:
                continue
            org = row[1].strip()
            if 0 <= val <= 0xFFFF and org and val not in entries:
                entries[val] = org
    return entries


def write_oui_bin(entries: dict[int, str], out_path: Path) -> int:
    keys = sorted(entries)
    out = bytearray()
    out += struct.pack("<4sIHHI", b"OUI1", len(keys), RECORD_SIZE, OUI_KEY_SIZE, 0)
    for k in keys:
        out += bytes([(k >> 16) & 0xFF, (k >> 8) & 0xFF, k & 0xFF])
        out += sanitize_ascii(entries[k], RECORD_SIZE - OUI_KEY_SIZE)
    out_path.write_bytes(out)
    return len(keys)


def write_bt_bin(entries: dict[int, str], out_path: Path) -> int:
    keys = sorted(entries)
    out = bytearray()
    out += struct.pack("<4sIHHI", b"BTM1", len(keys), RECORD_SIZE, BT_KEY_SIZE, 0)
    for k in keys:
        out += struct.pack("<H", k)
        out += sanitize_ascii(entries[k], RECORD_SIZE - BT_KEY_SIZE)
    out_path.write_bytes(out)
    return len(keys)


def run_selftest(cpp_bin: str | None) -> int:
    with tempfile.TemporaryDirectory(prefix="oscilla-vendor-") as td:
        d = Path(td)
        oui_csv = d / "oui.csv"
        oui_csv.write_text(
            "Registry,Assignment,Organization Name,Organization Address\n"
            "MA-L,001A2B,Synthetic Radio Corp,Lab\n"
            "MA-L,FCF5C4,\"Espressif, Inc.\",Shanghai\n"
            "MA-L,DC:A6:32,Raspberry Pi Trading,UK\n"
            "BAD,ZZZZZZ,Ignored\n",
            encoding="utf-8",
        )
        bt_yaml = d / "company_identifiers.yaml"
        bt_yaml.write_text(
            "company_identifiers:\n"
            "  - value: 0x004C\n"
            "    name: 'Apple, Inc.'\n"
            "  - value: 0x0075\n"
            "    name: Samsung Electronics\n"
            "  - value: 0x00E0\n"
            "    name: Google\n",
            encoding="utf-8",
        )
        oui_bin = d / "oui.bin"
        bt_bin = d / "bt_mfr.bin"
        assert write_oui_bin(parse_oui_csv(oui_csv), oui_bin) == 3
        assert write_bt_bin(parse_bt_file(bt_yaml), bt_bin) == 3
        assert oui_bin.stat().st_size == HEADER_SIZE + 3 * RECORD_SIZE
        assert bt_bin.stat().st_size == HEADER_SIZE + 3 * RECORD_SIZE

        if cpp_bin:
            def query(flag: str, table: Path, key: str) -> str:
                return subprocess.run(
                    [cpp_bin, flag, str(table), key], capture_output=True, text=True, check=True
                ).stdout.strip()

            assert query("--lookup-oui", oui_bin, "fc:f5:c4:11:22:33") == "Espressif, Inc."
            assert query("--lookup-oui", oui_bin, "00:1a:2b:ff:ee:dd") == "Synthetic Radio Corp"
            assert query("--lookup-oui", oui_bin, "dc:a6:32:01:02:03") == "Raspberry Pi Trading"
            assert query("--lookup-oui", oui_bin, "02:00:00:00:00:01") == ""
            assert query("--lookup-oui", oui_bin, "11:22:33:44:55:66") == ""
            assert query("--lookup-bt", bt_bin, "004c") == "Apple, Inc."
            assert query("--lookup-bt", bt_bin, "0x0075") == "Samsung Electronics"
            assert query("--lookup-bt", bt_bin, "00e0") == "Google"
            assert query("--lookup-bt", bt_bin, "ffff") == ""
    print("vendor tables: builder + C++ binary search cross-check passed")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--oui", type=Path, help="Input IEEE OUI CSV file")
    ap.add_argument("--out-oui", type=Path, help="Output /oscilla/oui.bin path")
    ap.add_argument("--bt", type=Path, help="Input Bluetooth SIG company_identifiers YAML or CSV")
    ap.add_argument("--out-bt", type=Path, help="Output /oscilla/bt_mfr.bin path")
    ap.add_argument("--selftest", action="store_true", help="Run round-trip self-test")
    ap.add_argument("--cpp-bin", help="Optional host vendor_table_test binary for cross-check")
    args = ap.parse_args()

    if args.selftest:
        return run_selftest(args.cpp_bin)
    if args.oui and args.out_oui:
        n = write_oui_bin(parse_oui_csv(args.oui), args.out_oui)
        print(f"wrote {n} OUI records -> {args.out_oui} ({args.out_oui.stat().st_size} bytes)")
    if args.bt and args.out_bt:
        n = write_bt_bin(parse_bt_file(args.bt), args.out_bt)
        print(f"wrote {n} BT company records -> {args.out_bt} ({args.out_bt.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
