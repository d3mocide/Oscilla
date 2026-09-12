#!/usr/bin/env bash
# check_protocol.sh — P0 exit gate, part 1: protocol/ocp.h compiles standalone
# and holds its invariants. Needs only a host C/C++ compiler.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d); trap 'rm -rf "$out"' EXIT
warn=(-Wall -Wextra -pedantic -Werror)

# The header must be clean in every standard either firmware might use.
for std in c99 c11 c17; do
    "${CC:-gcc}" -std=$std "${warn[@]}" -Iprotocol -o "$out/t.$std" protocol/test_ocp_header.c
    echo "  compiled: ${CC:-gcc} -std=$std"
done
for std in c++11 c++17; do
    "${CXX:-g++}" -std=$std "${warn[@]}" -Iprotocol -x c++ -o "$out/t.$std" protocol/test_ocp_header.c
    echo "  compiled: ${CXX:-g++} -std=$std"
done

# The transmit tripwire must actually trip (DESIGN §8, D-8).
if "${CC:-gcc}" -std=c11 -Iprotocol -DOSCILLA_LORA_TX -fsyntax-only protocol/test_ocp_header.c 2>/dev/null; then
    echo "FAIL: -DOSCILLA_LORA_TX did not trip the receive-only tripwire" >&2
    exit 1
fi
echo "  tripwire: -DOSCILLA_LORA_TX refuses to build (expected)"

"$out/t.c11"

# --- P0 exit gate, part 2: both firmware skeletons compile against ocp.h ----
# This is NOT a board build (no ESP-IDF, no PlatformIO here). It proves only
# that each firmware's use of the shared contract is well-formed C/C++.

"${CC:-gcc}" -std=c11 "${warn[@]}" -Iprotocol -c -o "$out/probe.o" \
    firmware-c5/main/main.c
echo "  compiled: firmware-c5/main/main.c against ocp.h"

"${CXX:-g++}" -std=gnu++17 "${warn[@]}" -Iprotocol -Itools/hoststub \
    -c -o "$out/deck.o" firmware-cardputer/src/main.cpp
echo "  compiled: firmware-cardputer/src/main.cpp against ocp.h"

echo "protocol contract OK"
