/*
 * config_codec.c — see config_codec.h.
 *
 * Blob layout (128 bytes, little-endian, explicit byte packing):
 *   [0..3]    magic "OCFG"
 *   [4..5]    version = 1
 *   [6..7]    payload_len = 116
 *   [8..15]   wifi_region[8]
 *   [16]      wifi_band
 *   [17]      lora_valid
 *   [18]      lora_sf
 *   [19]      lora_cr
 *   [20]      lora_sync_word
 *   [21]      reserved (0)
 *   [22..23]  lora_bw_khz
 *   [24..27]  lora_freq_hz
 *   [28..123] wifi_channels[96]
 *   [124..127] IEEE CRC32 over bytes [0..123]
 *
 * SPDX-License-Identifier: MIT
 */

#include "config_codec.h"

#include <string.h>

#include "ocp.h"
#include "ocp_wifi_regions.h"
#include "wifi_channels.h"

#define CFG_MAGIC_0      0x4Fu /* 'O' */
#define CFG_MAGIC_1      0x43u /* 'C' */
#define CFG_MAGIC_2      0x46u /* 'F' */
#define CFG_MAGIC_3      0x47u /* 'G' */
#define CFG_VERSION      1u
#define CFG_PAYLOAD_LEN  116u
#define CFG_CRC_OFFSET   124u

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t crc32_ieee(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static bool has_nul(const char *buf, size_t cap)
{
    return memchr(buf, '\0', cap) != NULL;
}

static bool valid_lora_bw(uint16_t khz)
{
    switch (khz) {
        case 7: case 10: case 15: case 20: case 31:
        case 41: case 62: case 125: case 250: case 500:
            return true;
        default:
            return false;
    }
}

static bool channel_in_band(uint8_t ch, uint8_t band)
{
    if (band == WIFI_BAND_SCOPE_24) return ch <= 14;
    if (band == WIFI_BAND_SCOPE_5)  return ch > 14;
    return true;
}

static bool channel_in_region(const ocp_wifi_region_t *r, uint8_t ch)
{
    for (size_t i = 0; i < r->count; i++) {
        if (r->channels[i] == ch) return true;
    }
    return false;
}

static bool valid_channels_csv(const ocp_wifi_region_t *r, uint8_t band, const char *s)
{
    if (s[0] == '\0' || strcmp(s, OCP_CHANNELS_ALL) == 0) return true;

    uint8_t seen[OCP_WIFI_REGION_MAX_CHANNELS];
    size_t count = 0;
    const char *p = s;

    for (;;) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len == 0 || len > 3) return false;
        unsigned v = 0;
        for (size_t i = 0; i < len; i++) {
            if (p[i] < '0' || p[i] > '9') return false;
            v = v * 10u + (unsigned)(p[i] - '0');
        }
        if (v == 0 || v > 255u) return false;
        uint8_t ch = (uint8_t)v;
        if (!channel_in_region(r, ch) || !channel_in_band(ch, band)) return false;
        for (size_t i = 0; i < count; i++) {
            if (seen[i] == ch) return false;
        }
        if (count >= OCP_WIFI_REGION_MAX_CHANNELS) return false;
        seen[count++] = ch;
        if (!comma) break;
        p = comma + 1;
    }
    return count > 0;
}

void probe_config_defaults(probe_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof *cfg);
    strncpy(cfg->wifi_region, OCP_WIFI_REGION_DEFAULT, sizeof cfg->wifi_region - 1);
    cfg->wifi_band = (uint8_t)WIFI_BAND_SCOPE_AUTO;
    cfg->wifi_channels[0] = '\0';
    cfg->lora_valid = 0;
}

bool probe_config_validate(const probe_config_t *cfg)
{
    if (!cfg) return false;
    if (!has_nul(cfg->wifi_region, sizeof cfg->wifi_region)) return false;
    if (!has_nul(cfg->wifi_channels, sizeof cfg->wifi_channels)) return false;

    const ocp_wifi_region_t *r = ocp_wifi_region_find(cfg->wifi_region);
    if (!r) return false;
    if (cfg->wifi_band > (uint8_t)WIFI_BAND_SCOPE_5) return false;
    if (!valid_channels_csv(r, cfg->wifi_band, cfg->wifi_channels)) return false;

    if (cfg->lora_valid > 1u) return false;
    if (cfg->lora_valid == 1u) {
        if (cfg->lora_freq_hz < 150000000UL || cfg->lora_freq_hz > 960000000UL) return false;
        if (cfg->lora_sf < 5u || cfg->lora_sf > 12u) return false;
        if (!valid_lora_bw(cfg->lora_bw_khz)) return false;
        if (cfg->lora_cr < 1u || cfg->lora_cr > 4u) return false;
    }
    return true;
}

bool probe_config_encode(const probe_config_t *cfg, uint8_t out[PROBE_CONFIG_BLOB_SIZE])
{
    if (!out || !probe_config_validate(cfg)) return false;
    memset(out, 0, PROBE_CONFIG_BLOB_SIZE);

    out[0] = CFG_MAGIC_0;
    out[1] = CFG_MAGIC_1;
    out[2] = CFG_MAGIC_2;
    out[3] = CFG_MAGIC_3;
    write_u16_le(out + 4, CFG_VERSION);
    write_u16_le(out + 6, CFG_PAYLOAD_LEN);

    size_t rlen = strlen(cfg->wifi_region);
    memcpy(out + 8, cfg->wifi_region, rlen);
    out[16] = cfg->wifi_band;
    out[17] = cfg->lora_valid;
    if (cfg->lora_valid) {
        out[18] = cfg->lora_sf;
        out[19] = cfg->lora_cr;
        out[20] = cfg->lora_sync_word;
        write_u16_le(out + 22, cfg->lora_bw_khz);
        write_u32_le(out + 24, cfg->lora_freq_hz);
    }
    size_t clen = strlen(cfg->wifi_channels);
    memcpy(out + 28, cfg->wifi_channels, clen);

    write_u32_le(out + CFG_CRC_OFFSET, crc32_ieee(out, CFG_CRC_OFFSET));
    return true;
}

bool probe_config_decode(const uint8_t *blob, size_t len, probe_config_t *out)
{
    if (!out) return false;
    probe_config_defaults(out);
    if (!blob || len != PROBE_CONFIG_BLOB_SIZE) return false;

    if (blob[0] != CFG_MAGIC_0 || blob[1] != CFG_MAGIC_1 ||
        blob[2] != CFG_MAGIC_2 || blob[3] != CFG_MAGIC_3) {
        return false;
    }
    if (read_u16_le(blob + 4) != CFG_VERSION ||
        read_u16_le(blob + 6) != CFG_PAYLOAD_LEN) {
        return false;
    }
    if (read_u32_le(blob + CFG_CRC_OFFSET) != crc32_ieee(blob, CFG_CRC_OFFSET)) {
        return false;
    }

    probe_config_t tmp;
    memset(&tmp, 0, sizeof tmp);
    memcpy(tmp.wifi_region, blob + 8, sizeof tmp.wifi_region);
    tmp.wifi_band = blob[16];
    tmp.lora_valid = blob[17];
    if (tmp.lora_valid == 1u) {
        tmp.lora_sf = blob[18];
        tmp.lora_cr = blob[19];
        tmp.lora_sync_word = blob[20];
        tmp.lora_bw_khz = read_u16_le(blob + 22);
        tmp.lora_freq_hz = read_u32_le(blob + 24);
    }
    memcpy(tmp.wifi_channels, blob + 28, sizeof tmp.wifi_channels);

    if (!probe_config_validate(&tmp)) return false;
    *out = tmp;
    return true;
}
