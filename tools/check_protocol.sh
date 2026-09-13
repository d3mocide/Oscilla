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

# The deck's connection client against a scripted probe.
"${CC:-gcc}" -std=c99 "${warn[@]}" -Iprotocol -c -o "$out/ocp_text.o" protocol/ocp_text.c
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/client_test" \
    firmware-cardputer/test/host/client_test.cpp firmware-cardputer/src/ocp/ocp_client.cpp \
    firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/client_test" | tail -1 | sed 's/^/  /'

# The deck's scan model: paging, validation, caps.
"${CXX:-g++}" -std=c++17 "${warn[@]}" -Iprotocol -Ifirmware-cardputer/src -o "$out/model_test" \
    firmware-cardputer/test/host/model_test.cpp firmware-cardputer/src/model/scan_model.cpp \
    firmware-cardputer/src/ocp/ocp_csv.cpp firmware-cardputer/src/ocp/ocp_parser.cpp "$out/ocp_text.o"
"$out/model_test" | tail -1 | sed 's/^/  /'

echo "protocol contract OK"
