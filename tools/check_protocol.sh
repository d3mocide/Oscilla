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

# The deck skeleton compiles against ocp.h on the host. The probe is a real
# ESP-IDF app now; tools/build_firmware.sh is what verifies it.

"${CXX:-g++}" -std=gnu++17 "${warn[@]}" -Iprotocol -Itools/hoststub \
    -c -o "$out/deck.o" firmware-cardputer/src/main.cpp
echo "  compiled: firmware-cardputer/src/main.cpp against ocp.h"

echo "protocol contract OK"
