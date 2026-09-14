/*
 * lora_framing.h — best-effort structural guess at what's inside a received
 * LoRa application payload: Meshtastic, LoRaWAN, or unknown (DESIGN §9.1's
 * `framing_guess`). Framework-agnostic; host-tested in
 * test/host/lora_framing_test.cpp.
 *
 * This is a *guess*, named that way in DESIGN on purpose: with no shared key,
 * an encrypted mesh payload is opaque, so classification can only ever read
 * the parts of a frame that are never encrypted — a fixed-format header
 * (Meshtastic) or a spec-mandated first byte plus length invariants
 * (LoRaWAN). Both checks are structural only, cited against the actual
 * public source/spec below, not guessed:
 *
 *  - Meshtastic: `PacketHeader` is a 16-byte plaintext header before the
 *    encrypted payload (meshtastic/firmware, src/mesh/RadioInterface.h,
 *    `MESHTASTIC_HEADER_LENGTH` / `MAX_LORA_PAYLOAD_LEN`, fetched and read
 *    2026-09-14 — not assumed from memory). Its `flags`/`channel` bytes use
 *    every bit, so they carry no structural signal; the one near-certain
 *    marker is `to == NODENUM_BROADCAST` (`MeshTypes.h`: `UINT32_MAX`),
 *    little-endian on the wire, which appears on the very common
 *    broadcast/route-discovery/telemetry-broadcast traffic. Read as a
 *    32-bit field, a false positive on non-Meshtastic bytes needs all 4
 *    bytes to land 0xFF by chance: ~2^-32.
 *  - LoRaWAN: MHDR is the wire's first byte on every PHYPayload (LoRaWAN L2
 *    1.0.4 spec §4): `MType` (bits 7-5), RFU (bits 4-2, must be 0), `Major`
 *    (bits 1-0, must be 0 for LoRaWAN R1). Join Request/Accept then have
 *    exact lengths (23, and 17 or 33 with an optional CFList); data
 *    messages (Un/Confirmed Up/Down) carry FOptsLen in FCtrl's low nibble,
 *    which fixes a minimum total length. Checked against random bytes of
 *    the right length, the MHDR gate alone (5 constrained low bits, 6 of 8
 *    MType codes accepted) passes ~2.3% of the time — real, but far weaker
 *    than the Meshtastic broadcast marker, so it's checked second.
 *
 * Neither check can ever prove a negative: a unicast Meshtastic packet (no
 * broadcast `to`) has no distinguishing bit pattern and reads as Unknown.
 * That's an accepted, documented limitation, not a bug — see
 * test/host/lora_framing_test.cpp for what this can and can't tell apart.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace model {

enum class Framing : uint8_t { Unknown, Meshtastic, LoRaWAN };

const char *framingName(Framing f);

/* Raw application payload bytes (post-CRC, as the radio delivered them). */
Framing classifyLoraFrame(const uint8_t *payload, size_t len);

/* Convenience overload matching how LoraPacket stores the payload. Malformed
 * hex (odd length, non-hex characters) yields Unknown, never throws. */
Framing classifyLoraFrameHex(const std::string &hex);

}  // namespace model
