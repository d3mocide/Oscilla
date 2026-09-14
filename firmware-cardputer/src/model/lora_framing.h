/*
 * lora_framing.h — best-effort structural guess at what's inside a received
 * LoRa application payload: Meshtastic, LoRaWAN, MeshCore, or unknown
 * (DESIGN §9.1's `framing_guess` — MeshCore isn't in DESIGN's original
 * meshtastic|lorawan|unknown set, added 2026-09-14 because it's the only
 * traffic actually seen on the bench so far; see ROADMAP P3). Framework-
 * agnostic; host-tested in test/host/lora_framing_test.cpp.
 *
 * This is a *guess*, named that way in DESIGN on purpose: with no shared key,
 * an encrypted mesh payload is opaque, so classification can only ever read
 * the parts of a frame that are never encrypted — a fixed-format header
 * (Meshtastic, MeshCore) or a spec-mandated first byte plus length
 * invariants (LoRaWAN). All three checks are structural only, cited against
 * the actual public source/spec below, not guessed:
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
 *  - MeshCore: fetched meshcore-dev/MeshCore's `src/Packet.{h,cpp}`
 *    (`writeTo`/`readFrom`/`getRawLength`, 2026-09-14 — not assumed). Byte 0
 *    is `header`: route type (bits 0-1, all 4 values defined, no filter),
 *    payload type (bits 2-5: 0x00-0x0B and 0x0F defined today, 0x0C-0x0E
 *    not), payload version (bits 6-7: only `PAYLOAD_VER_1`=0 is implemented,
 *    1-3 are `FUTURE`/reserved in the fetched source). A `TRANSPORT_FLOOD`
 *    or `TRANSPORT_DIRECT` route is followed by 4 bytes of transport codes;
 *    then a packed `path_len` byte (`hash_size-1` in its top 2 bits —
 *    `hash_size==4` is explicitly rejected by their own `isValidPathLen` as
 *    reserved — `hash_count` in the bottom 6), then `hash_count*hash_size`
 *    path bytes (`MAX_PATH_SIZE`=64), then the payload (`MAX_PACKET_PAYLOAD`
 *    =184), total capped at `MAX_TRANS_UNIT`=255 (`MeshCore.h`). The header
 *    gate alone is weaker than LoRaWAN's (ver+type-gap constrains ~1/4 *
 *    13/16 ≈ 20% of random bytes through), tightened further — not fully
 *    quantified — by requiring the path/payload lengths to exactly consume
 *    the rest of the frame. Checked last: on real MeshCore traffic, this is
 *    the one that should actually fire; on anything else, it's the weakest
 *    of the three gates and most likely to produce a false positive, so
 *    Meshtastic and LoRaWAN get first refusal.
 *
 * None of the three checks can ever prove a negative: a unicast Meshtastic
 * packet (no broadcast `to`) has no distinguishing bit pattern and reads as
 * Unknown. That's an accepted, documented limitation, not a bug — see
 * test/host/lora_framing_test.cpp for what this can and can't tell apart,
 * including the real (if small) chance of cross-talk between the LoRaWAN
 * and MeshCore gates on bytes that satisfy neither protocol.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace model {

enum class Framing : uint8_t { Unknown, Meshtastic, LoRaWAN, MeshCore };

const char *framingName(Framing f);

/* Raw application payload bytes (post-CRC, as the radio delivered them). */
Framing classifyLoraFrame(const uint8_t *payload, size_t len);

/* Convenience overload matching how LoraPacket stores the payload. Malformed
 * hex (odd length, non-hex characters) yields Unknown, never throws. */
Framing classifyLoraFrameHex(const std::string &hex);

}  // namespace model
