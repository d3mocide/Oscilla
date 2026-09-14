/*
 * lora_framing.cpp — see lora_framing.h for the structural rules and their
 * citations.
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/lora_framing.h"

namespace model {

namespace {

constexpr size_t kMeshtasticHeaderLen = 16;   /* MESHTASTIC_HEADER_LENGTH */
constexpr size_t kMaxLoraPayloadLen = 255;    /* MAX_LORA_PAYLOAD_LEN */

bool looksMeshtastic(const uint8_t *p, size_t len)
{
    if (len < kMeshtasticHeaderLen || len > kMaxLoraPayloadLen) return false;
    /* PacketHeader.to, little-endian, == NODENUM_BROADCAST (UINT32_MAX). */
    return p[0] == 0xFF && p[1] == 0xFF && p[2] == 0xFF && p[3] == 0xFF;
}

bool looksLoRaWAN(const uint8_t *p, size_t len)
{
    if (len < 1) return false;
    uint8_t mhdr = p[0];
    uint8_t mtype = (mhdr >> 5) & 0x07;
    uint8_t rfu = (mhdr >> 2) & 0x07;
    uint8_t major = mhdr & 0x03;
    if (rfu != 0 || major != 0) return false;

    switch (mtype) {
    case 0:   /* Join Request: MHDR + AppEUI(8) + DevEUI(8) + DevNonce(2) + MIC(4) */
        return len == 23;
    case 1:   /* Join Accept: MHDR + [16 or 32 encrypted bytes incl. MIC] */
        return len == 17 || len == 33;
    case 2:   /* Unconfirmed Data Up */
    case 3:   /* Unconfirmed Data Down */
    case 4:   /* Confirmed Data Up */
    case 5: { /* Confirmed Data Down */
        /* FHDR = DevAddr(4) + FCtrl(1) + FCnt(2) + FOpts(FOptsLen); need
         * index 5 (FCtrl) in range before reading it. */
        if (len < 1 + 7) return false;
        uint8_t fctrl = p[5];
        uint8_t fopts_len = fctrl & 0x0F;
        size_t min_len = 1 + 7 + fopts_len + 4;   /* MHDR+FHDR+FOpts+MIC, no FPort/payload */
        return len >= min_len;
    }
    default:   /* 6 = RFU, 7 = Proprietary: not a standard frame we can name */
        return false;
    }
}

int hexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

const char *framingName(Framing f)
{
    switch (f) {
    case Framing::Meshtastic: return "meshtastic";
    case Framing::LoRaWAN: return "lorawan";
    default: return "unknown";
    }
}

Framing classifyLoraFrame(const uint8_t *payload, size_t len)
{
    /* Checked in order of ascending false-positive rate (see header):
     * Meshtastic's broadcast marker first, LoRaWAN's MHDR gate second. */
    if (looksMeshtastic(payload, len)) return Framing::Meshtastic;
    if (looksLoRaWAN(payload, len)) return Framing::LoRaWAN;
    return Framing::Unknown;
}

Framing classifyLoraFrameHex(const std::string &hex)
{
    if (hex.empty() || hex.size() % 2 != 0) return Framing::Unknown;
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = hexNibble(hex[i]), lo = hexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return Framing::Unknown;
        bytes.push_back(static_cast<char>((hi << 4) | lo));
    }
    return classifyLoraFrame(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
}

}  // namespace model
