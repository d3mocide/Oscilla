#!/usr/bin/env python3
"""Mutation tests for the receive-only source gate (D-8)."""

import importlib.util
import shutil
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("check_rx_only", ROOT / "tools/check_rx_only.py")
assert SPEC and SPEC.loader
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


def checkout() -> tempfile.TemporaryDirectory:
    tmp = tempfile.TemporaryDirectory(prefix="oscilla-rx-only-")
    base = Path(tmp.name)
    for rel in ("firmware-c5", "firmware-cardputer", "protocol"):
        shutil.copytree(ROOT / rel, base / rel)
    return tmp


def expect_pass(root: Path) -> None:
    if CHECK.main(root) != 0:
        raise AssertionError("unmodified receive-only source must pass")


def expect_fail(root: Path, rel: str, old: str, new: str, label: str) -> None:
    path = root / rel
    text = path.read_text()
    if old not in text:
        raise AssertionError(f"mutation source not found: {label}")
    path.write_text(text.replace(old, new, 1))
    if CHECK.main(root) == 0:
        raise AssertionError(f"receive-only check missed mutation: {label}")


def main() -> int:
    with checkout() as tmp:
        root = Path(tmp)
        expect_pass(root)

    mutations = [
        ("firmware-c5/main/wifi_recon.c",
         ".scan_type = WIFI_SCAN_TYPE_PASSIVE",
         ".scan_time.passive = WIFI_SCAN_DWELL_MS",
         "Wi-Fi passive initializer removed"),
        ("firmware-c5/main/ble_recon.c",
         "params.passive = 1",
         "params.passive = 0",
         "BLE passive mode disabled"),
        ("firmware-c5/main/lora_radio.c",
         "#define OP_SET_SLEEP              0x84",
         "#define OP_SET_TX                0x83\n#define OP_SET_SLEEP              0x84\nstatic void forbidden_tx_opcode(void) { cmd_write(OP_SET_TX, NULL, 0); }",
         "SX1262 SetTx opcode introduced"),
        ("firmware-c5/main/lora_radio.c",
         "return write_register(REG_LORA_SYNC_WORD_MSB, data, sizeof data);",
         "return write_register(0x08D8, data, sizeof data);",
         "SX1262 register write to a non-allowlisted address"),
        ("firmware-c5/main/lora_radio.c",
         "#define REG_LORA_SYNC_WORD_MSB    0x0740",
         "#define REG_LORA_SYNC_WORD_MSB    0x08E7",
         "SX1262 allowlisted register name repointed at another address"),
        ("firmware-c5/main/zig_radio.c",
         "esp_ieee802154_set_promiscuous(true)",
         "esp_ieee802154_set_promiscuous(false)",
         "802.15.4 promiscuous receive disabled"),
    ]
    for rel, old, new, label in mutations:
        with checkout() as tmp:
            expect_fail(Path(tmp), rel, old, new, label)
    print(f"receive-only mutation tests OK: baseline plus {len(mutations)} rejected mutations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
