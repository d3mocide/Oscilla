#!/usr/bin/env bash
# check_protocol.sh — ocp.h compiles standalone and holds its invariants.
# Host compiler only; no toolchain needed.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d); trap 'rm -rf "$out"' EXIT
warn=(-Wall -Wextra -pedantic -Werror)

# Clean in every standard either firmware might use.
for std in c99 c11 c17; do
    "${CC:-gcc}" -std=$std "${warn[@]}" -Iprotocol -o "$out/t.$std" protocol/test_ocp_header.c
    echo "  compiled: ${CC:-gcc} -std=$std"
done
for std in c++11 c++17; do
    "${CXX:-g++}" -std=$std "${warn[@]}" -Iprotocol -x c++ -o "$out/t.$std" protocol/test_ocp_header.c
    echo "  compiled: ${CXX:-g++} -std=$std"
done

# The transmit tripwire must actually trip (D-8).
if "${CC:-gcc}" -std=c11 -Iprotocol -DOSCILLA_LORA_TX -fsyntax-only protocol/test_ocp_header.c 2>/dev/null; then
    echo "FAIL: -DOSCILLA_LORA_TX did not trip the receive-only tripwire" >&2
    exit 1
fi
echo "  tripwire: -DOSCILLA_LORA_TX refuses to build (expected)"

"$out/t.c11"

# The C field encoder must agree with the Python reference byte-for-byte.
"${CC:-gcc}" -std=c99 "${warn[@]}" -Iprotocol -o "$out/text_corpus" \
    protocol/ocp_text.c protocol/test_ocp_text.c
python3 tools/check_ocp_text.py "$out/text_corpus"

# D-8 at the driver level: no transmit-capable API in firmware source.
python3 tools/check_rx_only.py
python3 tools/test_check_rx_only.py | tail -1 | sed 's/^/  /'
python3 tools/test_zig_readiness.py | tail -1 | sed 's/^/  /'
python3 tools/check_event_queue.py | tail -1 | sed 's/^/  /'
python3 tools/test_wifi_inspect_arm.py | tail -1 | sed 's/^/  /'
python3 tools/test_sweep_layout.py | tail -1 | sed 's/^/  /'
python3 tools/test_card_selection_style.py | tail -1 | sed 's/^/  /'

# Spec rules: the OCP-SPEC §9 conformance checklist.
python3 tools/ocp_repl.py --selftest --no-color | tail -1 | sed 's/^/  /'

# Parser properties: never raises, chunk-invariant, noise-safe, recovers, bounded.
python3 tools/ocp_fuzz.py --iterations 1000 | grep -E 'FAIL|reproduce|fuzz:' | sed 's/^/  /'

# The deck's C++ parser must agree with the reference, item for item.
python3 tools/check_deck_parser.py --count 150

# Probe beacon parser: known answers + mutation fuzz under ASan/UBSan.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/beacon_test" firmware-c5/test/host/beacon_test.c firmware-c5/main/beacon_parse.c
"$out/beacon_test" 100000 | tail -1 | sed 's/^/  /'

# LoRa event hex encoding: zero-length packets must still produce an empty,
# terminated field, and undersized output must fail without partial writes.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/lora_hex_test" \
    firmware-c5/test/host/lora_hex_test.c firmware-c5/main/lora_hex.c
"$out/lora_hex_test" | tail -1 | sed 's/^/  /'

# Block frames are buffered and committed as one transaction; unavailable or
# oversized output must never reach the transport partially.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/ocp_block_test" \
    firmware-c5/test/host/ocp_block_test.c firmware-c5/main/ocp_block.c
"$out/ocp_block_test" | tail -1 | sed 's/^/  /'

# Command numeric fields use whole-token parsing with overflow protection.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/ocp_parse_test" \
    firmware-c5/test/host/ocp_parse_test.c firmware-c5/main/ocp_parse.c
"$out/ocp_parse_test" | tail -1 | sed 's/^/  /'

# Probe request parser (sniffer's SSID-tracking path): same treatment.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/probe_parse_test" firmware-c5/test/host/probe_parse_test.c firmware-c5/main/probe_parse.c
"$out/probe_parse_test" 100000 | tail -1 | sed 's/^/  /'

# BLE advertisement AD-structure parser (device name/manufacturer data,
# tracker classification): same treatment.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/ble_adv_parse_test" firmware-c5/test/host/ble_adv_parse_test.c firmware-c5/main/ble_adv_parse.c
"$out/ble_adv_parse_test" 100000 | tail -1 | sed 's/^/  /'

# BLE device table: upsert-by-address logic `scan_bt`'s reply is built from.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/ble_device_table_test" \
    firmware-c5/test/host/ble_device_table_test.c firmware-c5/main/ble_device_table.c firmware-c5/main/ble_adv_parse.c
"$out/ble_device_table_test" | tail -1 | sed 's/^/  /'

# 802.15.4's hostile MAC parser and capped PAN/node table (D-17).
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -Ifirmware-c5/main -Iprotocol -o "$out/zig_frame_test" \
    firmware-c5/test/host/zig_frame_test.c firmware-c5/main/zig_frame.c firmware-c5/main/zig_table.c
"$out/zig_frame_test" | tail -1 | sed 's/^/  /'

# Sniffer tracking tables (AP<->client, probe SSIDs): dedup/overflow/address
# classification, with no radio or hardware needed.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/sniff_track_test" firmware-c5/test/host/sniff_track_test.c firmware-c5/main/sniff_track.c
"$out/sniff_track_test" 100000 | tail -1 | sed 's/^/  /'

# Continuous Wi-Fi AP/BSSID table: dedup, latest observation and bounded
# overflow behavior, with no radio or hardware needed.
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/wifi_network_table_test" \
    firmware-c5/test/host/wifi_network_table_test.c firmware-c5/main/wifi_network_table.c
"$out/wifi_network_table_test" | tail -1 | sed 's/^/  /'

# Deauth/disassoc frame parser (deauth_detector's classification path).
"${CC:-gcc}" -std=c99 -g -O1 "${warn[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Ifirmware-c5/main -o "$out/deauth_parse_test" firmware-c5/test/host/deauth_parse_test.c firmware-c5/main/deauth_parse.c
"$out/deauth_parse_test" 100000 | tail -1 | sed 's/^/  /'

# The deck's connection client against a scripted probe.
"${CC:-gcc}" -std=c99 "${warn[@]}" -Iprotocol -c -o "$out/ocp_text.o" protocol/ocp_text.c
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/client_test" \
    firmware-cardputer/test/host/client_test.cpp firmware-cardputer/src/ocp/ocp_client.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/client_test" | tail -1 | sed 's/^/  /'

# The deck's grouped route order and labels: navigation must stay separate from
# command orchestration so UI regrouping cannot change radio behavior.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/navigation_test" \
    firmware-cardputer/test/host/navigation_test.cpp firmware-cardputer/src/app/deck_navigation.cpp
"$out/navigation_test" | tail -1 | sed 's/^/  /'

# A PHY handoff must wait for [STOP] before it starts the requested successor.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/phy_handoff_test" \
    firmware-cardputer/test/host/phy_handoff_test.cpp firmware-cardputer/src/app/phy_handoff.cpp
"$out/phy_handoff_test" | tail -1 | sed 's/^/  /'

# Packet-monitor teardown must leave Wi-Fi ready for the next passive engine.
python3 tools/test_wifi_spectrum_teardown.py | tail -1 | sed 's/^/  /'

# 802.15.4 teardown must queue a recovery reboot, held off while LoRa RX
# runs, with the [STOP] ack always reaching the deck first.
python3 tools/test_probe_restart.py | tail -1 | sed 's/^/  /'

# The deck's scan model: paging, validation, caps.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/model_test" \
    firmware-cardputer/test/host/model_test.cpp firmware-cardputer/src/model/scan_model.cpp \
    firmware-cardputer/src/ocp/ocp_csv.cpp firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/model_test" | tail -1 | sed 's/^/  /'

# The deck's contacts model: [CLIENTS]/[PROBES] snapshots vs. the event ticker.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/contacts_model_test" \
    firmware-cardputer/test/host/contacts_model_test.cpp firmware-cardputer/src/model/contacts_model.cpp \
    firmware-cardputer/src/ocp/ocp_csv.cpp firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/contacts_model_test" | tail -1 | sed 's/^/  /'

# The deck's BLE model: [BLE] device table vs. the scan_airtag tracker log.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/bt_model_test" \
    firmware-cardputer/test/host/bt_model_test.cpp firmware-cardputer/src/model/bt_model.cpp \
    firmware-cardputer/src/ocp/ocp_csv.cpp firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/bt_model_test" | tail -1 | sed 's/^/  /'

# The deck-local anti-surveillance correlator: bounded tracker table, fresh-fix
# requirement, two-leg movement threshold, and stop/clear semantics.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/anti_surveillance_model_test" \
    firmware-cardputer/test/host/anti_surveillance_model_test.cpp \
    firmware-cardputer/src/model/anti_surveillance_model.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/anti_surveillance_model_test" | tail -1 | sed 's/^/  /'

"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/zig_model_test" \
    firmware-cardputer/test/host/zig_model_test.cpp firmware-cardputer/src/model/zig_model.cpp \
    firmware-cardputer/src/ocp/ocp_csv.cpp firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/zig_model_test" | tail -1 | sed 's/^/  /'

# The deck's spectrum model: channel_view/packet_monitor event absorption.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/spectrum_model_test" \
    firmware-cardputer/test/host/spectrum_model_test.cpp firmware-cardputer/src/model/spectrum_model.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/spectrum_model_test" | tail -1 | sed 's/^/  /'

# The deck's deauth model: kind=deauth event log, capped and validated.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/deauth_model_test" \
    firmware-cardputer/test/host/deauth_model_test.cpp firmware-cardputer/src/model/deauth_model.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/deauth_model_test" | tail -1 | sed 's/^/  /'

# The deck's LoRa model: kind=lora packet log, capped and validated.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/lora_model_test" \
    firmware-cardputer/test/host/lora_model_test.cpp firmware-cardputer/src/model/lora_model.cpp \
    firmware-cardputer/src/model/lora_framing.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/lora_model_test" | tail -1 | sed 's/^/  /'

# The deck's LoRa framing classifier: Meshtastic/LoRaWAN structural guesses.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/lora_framing_test" \
    firmware-cardputer/test/host/lora_framing_test.cpp firmware-cardputer/src/model/lora_framing.cpp
"$out/lora_framing_test" | tail -1 | sed 's/^/  /'

# The deck's LoRa session log row format (no SD I/O - pure formatting only).
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/lora_log_format_test" \
    firmware-cardputer/test/host/lora_log_format_test.cpp firmware-cardputer/src/storage/lora_log_format.cpp \
    firmware-cardputer/src/model/lora_framing.cpp
"$out/lora_log_format_test" | tail -1 | sed 's/^/  /'

# LoRa session sidecars: configuration provenance and health rows stay pure
# and testable without an SD card.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/lora_session_format_test" \
    firmware-cardputer/test/host/lora_session_format_test.cpp firmware-cardputer/src/storage/lora_session_format.cpp \
    firmware-cardputer/src/model/lora_model.cpp firmware-cardputer/src/model/lora_framing.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/lora_session_format_test" | tail -1 | sed 's/^/  /'

# Named LoRa profile presets: pins the sourced values (LoRaTrace-RX's
# channel_plans.h) against drift.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/lora_profiles_test" \
    firmware-cardputer/test/host/lora_profiles_test.cpp
"$out/lora_profiles_test" | tail -1 | sed 's/^/  /'

# The deck's NMEA reader: checksum, chunking, bounds - no OCP link involved.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/nmea_parser_test" \
    firmware-cardputer/test/host/nmea_parser_test.cpp firmware-cardputer/src/gnss/nmea_parser.cpp
"$out/nmea_parser_test" | tail -1 | sed 's/^/  /'

# The deck's debug-console line reader: same chunk-boundary discipline as
# the NMEA reader above, for the other arbitrarily-chunked UART (USB Serial).
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/line_reader_test" \
    firmware-cardputer/test/host/line_reader_test.cpp firmware-cardputer/src/debug/line_reader.cpp
"$out/line_reader_test" | tail -1 | sed 's/^/  /'

# The deck's GNSS fix model: fix validity/age vs. no-UART-data, kept distinct.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/gnss_model_test" \
    firmware-cardputer/test/host/gnss_model_test.cpp firmware-cardputer/src/gnss/nmea_parser.cpp \
    firmware-cardputer/src/model/gnss_model.cpp
"$out/gnss_model_test" | tail -1 | sed 's/^/  /'

# The deck's WigleWifi-1.6 CSV row formatting for the wardrive log.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/wardrive_csv_test" \
    firmware-cardputer/test/host/wardrive_csv_test.cpp firmware-cardputer/src/storage/wardrive_csv.cpp
"$out/wardrive_csv_test" | tail -1 | sed 's/^/  /'

# The deck's KML track/placemark formatting for the wardrive log.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/wardrive_kml_test" \
    firmware-cardputer/test/host/wardrive_kml_test.cpp firmware-cardputer/src/storage/wardrive_kml.cpp
"$out/wardrive_kml_test" | tail -1 | sed 's/^/  /'
# Cross-checked against a real XML parser, not just string equality (same
# role check_ocp_text.py plays for the C field encoder).
python3 tools/check_wardrive_kml.py "$out/wardrive_kml_test"

# The deck's wardrive session discovery: exact filenames and first-free index,
# independent of SD I/O so the directory walk cannot regress silently.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Ifirmware-cardputer/src -o "$out/wardrive_sessions_test" \
    firmware-cardputer/test/host/wardrive_sessions_test.cpp firmware-cardputer/src/storage/wardrive_sessions.cpp
"$out/wardrive_sessions_test" | tail -1 | sed 's/^/  /'

echo "protocol contract OK"
