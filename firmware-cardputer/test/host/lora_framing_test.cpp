/*
 * lora_framing_test.cpp — host test for model::classifyLoraFrame(). Known-
 * answer fixtures built directly from the cited structures (Meshtastic's
 * PacketHeader, the LoRaWAN MHDR/FHDR layout), not from this same code.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "model/lora_framing.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

model::Framing classify(const std::vector<uint8_t> &v)
{
    return model::classifyLoraFrame(v.data(), v.size());
}

void appendLE32(std::vector<uint8_t> &v, uint32_t x)
{
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
}

std::vector<uint8_t> fill(size_t n, uint8_t v = 0x5A)
{
    return std::vector<uint8_t>(n, v);   /* "encrypted": opaque, content-agnostic */
}

/* Meshtastic PacketHeader (16B): to, from, id (LE32 each), flags, channel,
 * next_hop, relay_node — then an opaque application payload. */
std::vector<uint8_t> meshtasticFrame(uint32_t to, size_t payload_len)
{
    std::vector<uint8_t> v;
    appendLE32(v, to);
    appendLE32(v, 0x12345678);        /* from */
    appendLE32(v, 0xAABBCCDD);        /* id */
    v.push_back(0x83);                /* flags: hop_limit=3, want_ack=1 */
    v.push_back(0x2B);                /* channel hash */
    v.push_back(0x00);                /* next_hop */
    v.push_back(0x00);                /* relay_node */
    auto payload = fill(payload_len);
    v.insert(v.end(), payload.begin(), payload.end());
    return v;
}

std::string toHex(const std::vector<uint8_t> &v)
{
    static const char *digits = "0123456789abcdef";
    std::string s;
    s.reserve(v.size() * 2);
    for (uint8_t b : v) {
        s.push_back(digits[b >> 4]);
        s.push_back(digits[b & 0x0F]);
    }
    return s;
}

}  // namespace

int main()
{
    check(std::string(model::framingName(model::Framing::Meshtastic)) == "meshtastic" &&
          std::string(model::framingName(model::Framing::LoRaWAN)) == "lorawan" &&
          std::string(model::framingName(model::Framing::Unknown)) == "unknown",
          "framingName() maps all three values");

    check(classify({}) == model::Framing::Unknown, "empty payload: unknown, no crash");

    // --- Meshtastic ---
    {
        auto v = meshtasticFrame(0xFFFFFFFF, 20);   // NODENUM_BROADCAST
        check(v.size() == 36, "fixture sanity: 16B header + 20B payload");
        check(classify(v) == model::Framing::Meshtastic, "broadcast `to` + valid length -> meshtastic");
    }
    {
        auto v = meshtasticFrame(0xFFFFFFFF, 0);    // header only, empty payload
        check(classify(v) == model::Framing::Meshtastic, "header-only broadcast packet still classifies");
    }
    {
        auto v = meshtasticFrame(0x00000042, 20);   // ordinary unicast node number
        check(classify(v) == model::Framing::Unknown,
              "unicast Meshtastic (no broadcast marker) is a documented miss -> unknown, not misclassified as something else");
    }
    {
        std::vector<uint8_t> v = {0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
        check(v.size() == 14, "fixture sanity: shorter than the 16B header");
        check(classify(v) == model::Framing::Unknown, "broadcast marker present but too short for a header: unknown");
    }
    {
        auto v = meshtasticFrame(0xFFFFFFFF, 255);   // 16 + 255 > MAX_LORA_PAYLOAD_LEN (255)
        check(classify(v) == model::Framing::Unknown, "broadcast marker but total exceeds MAX_LORA_PAYLOAD_LEN: unknown");
    }

    // --- LoRaWAN ---
    {
        std::vector<uint8_t> v(23, 0);
        v[0] = 0x00;   // MHDR: MType=Join Request(0), RFU=0, Major=0
        check(classify(v) == model::Framing::LoRaWAN, "23B MHDR=0x00 -> Join Request");
    }
    {
        std::vector<uint8_t> v(22, 0);
        v[0] = 0x00;
        check(classify(v) == model::Framing::Unknown, "Join Request MHDR but wrong length (22, not 23): unknown");
    }
    {
        std::vector<uint8_t> v(24, 0);
        v[0] = 0x00;
        check(classify(v) == model::Framing::Unknown, "Join Request MHDR but wrong length (24, not 23): unknown");
    }
    {
        std::vector<uint8_t> v(17, 0);
        v[0] = 0x20;   // MType=Join Accept(1)<<5 = 0x20
        check(classify(v) == model::Framing::LoRaWAN, "17B MHDR=0x20 -> Join Accept, no CFList");
    }
    {
        std::vector<uint8_t> v(33, 0);
        v[0] = 0x20;
        check(classify(v) == model::Framing::LoRaWAN, "33B MHDR=0x20 -> Join Accept, with CFList");
    }
    {
        std::vector<uint8_t> v(18, 0);
        v[0] = 0x20;
        check(classify(v) == model::Framing::Unknown, "Join Accept MHDR but a length that's neither 17 nor 33: unknown");
    }
    {
        // Unconfirmed Data Up (MType=2 -> 0x40), FOptsLen=0, no FPort/payload: 12B minimum.
        std::vector<uint8_t> v(12, 0);
        v[0] = 0x40;
        v[5] = 0x00;   // FCtrl: FOptsLen=0
        check(classify(v) == model::Framing::LoRaWAN, "12B minimal Data Up frame (FOptsLen=0) -> lorawan");
    }
    {
        std::vector<uint8_t> v(11, 0);
        v[0] = 0x40;
        check(classify(v) == model::Framing::Unknown, "Data Up MHDR but below the 12B absolute floor: unknown");
    }
    {
        // FOptsLen=5 declared in FCtrl -> needs 17B minimum (12 + 5); 16B is short by one.
        std::vector<uint8_t> v(16, 0);
        v[0] = 0x40;
        v[5] = 0x05;
        check(classify(v) == model::Framing::Unknown, "FCtrl declares FOptsLen=5 but frame is one byte short of consistent: unknown");
    }
    {
        std::vector<uint8_t> v(17, 0);
        v[0] = 0x40;
        v[5] = 0x05;
        check(classify(v) == model::Framing::LoRaWAN, "same FOptsLen=5, now length-consistent: lorawan");
    }
    {
        // FCtrl's upper nibble (ADR/ADRACKReq/ACK/ClassB) must not leak into
        // FOptsLen: 0x85 = ADR set + FOptsLen=5, same as the plain-0x05 case.
        std::vector<uint8_t> v(17, 0);
        v[0] = 0x40;
        v[5] = 0x85;
        check(classify(v) == model::Framing::LoRaWAN, "FCtrl upper nibble set (ADR) alongside FOptsLen=5: still lorawan, nibble masked correctly");
    }
    {
        // Confirmed Data Down, MType=5 -> 0x80 | 0x20? MType=5 = 0b101 -> (5<<5)=0xA0.
        std::vector<uint8_t> v(12, 0);
        v[0] = 0xA0;
        check(classify(v) == model::Framing::LoRaWAN, "MType=5 (Confirmed Data Down) also recognized");
    }
    {
        std::vector<uint8_t> v(23, 0);
        v[0] = 0xC0;   // MType=6 (RFU)
        check(classify(v) == model::Framing::Unknown, "MType=6 (RFU) is not a standard frame: unknown");
    }
    {
        std::vector<uint8_t> v(23, 0);
        v[0] = 0xE0;   // MType=7 (Proprietary)
        check(classify(v) == model::Framing::Unknown, "MType=7 (Proprietary) is not a standard frame we can name: unknown");
    }
    {
        std::vector<uint8_t> v(23, 0);
        v[0] = 0x04;   // MType=0 but RFU bits nonzero (bits4-2 = 001)
        check(classify(v) == model::Framing::Unknown, "nonzero RFU bits in MHDR fail the gate even with an otherwise-valid MType");
    }
    {
        std::vector<uint8_t> v(23, 0);
        v[0] = 0x01;   // MType=0 but Major bits nonzero
        check(classify(v) == model::Framing::Unknown, "nonzero Major bits in MHDR fail the gate");
    }

    // --- Neither: realistic opaque/encrypted traffic (e.g. MeshCore, whose
    // framing this classifier makes no claim about) must land on Unknown,
    // not get mislabeled. Built to deterministically fail both gates rather
    // than relying on low probability at test time. ---
    {
        std::vector<uint8_t> v = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        // byte0=0x5A: rfu=(0x5A>>2)&7=6 (nonzero) -> fails LoRaWAN gate. Not
        // 0xFF-prefixed -> fails Meshtastic gate.
        check(classify(v) == model::Framing::Unknown, "opaque non-broadcast payload with a non-MHDR-shaped first byte: unknown");
    }

    // --- hex convenience overload ---
    {
        auto v = meshtasticFrame(0xFFFFFFFF, 20);
        check(model::classifyLoraFrameHex(toHex(v)) == model::Framing::Meshtastic,
              "classifyLoraFrameHex decodes and matches classifyLoraFrame on the same bytes");
    }
    check(model::classifyLoraFrameHex("abc") == model::Framing::Unknown, "odd-length hex: unknown, not a crash");
    check(model::classifyLoraFrameHex("zzzz") == model::Framing::Unknown, "non-hex characters: unknown, not a crash");
    check(model::classifyLoraFrameHex("") == model::Framing::Unknown, "empty hex string: unknown");

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "lora framing test FAILED" : "lora framing test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
