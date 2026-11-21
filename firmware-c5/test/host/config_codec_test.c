/* Host tests for config_codec.c (WP-5 probe NVS persistence). */
#include "config_codec.h"
#include "ocp.h"
#include "wifi_channels.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    probe_config_t cfg;
    probe_config_defaults(&cfg);
    assert(probe_config_validate(&cfg));
    assert(strcmp(cfg.wifi_region, OCP_WIFI_REGION_DEFAULT) == 0);
    assert(cfg.wifi_band == WIFI_BAND_SCOPE_AUTO);
    assert(cfg.wifi_channels[0] == '\0');
    assert(cfg.lora_valid == 0);

    /* Round-trip defaults. */
    uint8_t blob[PROBE_CONFIG_BLOB_SIZE];
    assert(probe_config_encode(&cfg, blob));
    probe_config_t out;
    assert(probe_config_decode(blob, sizeof blob, &out));
    assert(strcmp(out.wifi_region, OCP_WIFI_REGION_DEFAULT) == 0);
    assert(out.wifi_band == WIFI_BAND_SCOPE_AUTO);
    assert(out.lora_valid == 0);

    /* Round-trip full config: EU, 2.4 GHz, custom channels, MeshCore LoRa RX. */
    strncpy(cfg.wifi_region, "EU", sizeof cfg.wifi_region - 1);
    cfg.wifi_band = WIFI_BAND_SCOPE_24;
    strncpy(cfg.wifi_channels, "1,6,11,13", sizeof cfg.wifi_channels - 1);
    cfg.lora_valid = 1;
    cfg.lora_freq_hz = 910525000UL;
    cfg.lora_sf = 7;
    cfg.lora_bw_khz = 62;
    cfg.lora_cr = 1;
    cfg.lora_sync_word = 0x12;
    assert(probe_config_encode(&cfg, blob));
    assert(probe_config_decode(blob, sizeof blob, &out));
    assert(strcmp(out.wifi_region, "EU") == 0);
    assert(out.wifi_band == WIFI_BAND_SCOPE_24);
    assert(strcmp(out.wifi_channels, "1,6,11,13") == 0);
    assert(out.lora_valid == 1);
    assert(out.lora_freq_hz == 910525000UL);
    assert(out.lora_sf == 7 && out.lora_bw_khz == 62 && out.lora_cr == 1 && out.lora_sync_word == 0x12);

    /* Single-bit corruption anywhere in the blob is caught by CRC32 and falls back to defaults. */
    for (size_t pos = 0; pos < sizeof blob; pos += 17) {
        uint8_t bad[PROBE_CONFIG_BLOB_SIZE];
        memcpy(bad, blob, sizeof blob);
        bad[pos] ^= 0x04u;
        assert(!probe_config_decode(bad, sizeof bad, &out));
        assert(strcmp(out.wifi_region, OCP_WIFI_REGION_DEFAULT) == 0);
        assert(out.lora_valid == 0);
    }

    /* Wrong length / wrong magic / wrong version rejected. */
    assert(!probe_config_decode(blob, sizeof blob - 1, &out));
    assert(!probe_config_decode(NULL, sizeof blob, &out));

    /* Invalid region rejected. */
    probe_config_t bad_cfg = cfg;
    strncpy(bad_cfg.wifi_region, "ZZ", sizeof bad_cfg.wifi_region - 1);
    assert(!probe_config_validate(&bad_cfg));
    assert(!probe_config_encode(&bad_cfg, blob));

    /* Channel 13 invalid in US; DFS channel 52 invalid everywhere; 5 GHz ch invalid under band=24. */
    bad_cfg = cfg;
    strncpy(bad_cfg.wifi_region, "US", sizeof bad_cfg.wifi_region - 1);
    assert(!probe_config_validate(&bad_cfg)); /* ch 13 not in US */
    strncpy(bad_cfg.wifi_channels, "52", sizeof bad_cfg.wifi_channels - 1);
    bad_cfg.wifi_band = WIFI_BAND_SCOPE_AUTO;
    assert(!probe_config_validate(&bad_cfg)); /* DFS ch 52 banned */
    strncpy(bad_cfg.wifi_channels, "36", sizeof bad_cfg.wifi_channels - 1);
    bad_cfg.wifi_band = WIFI_BAND_SCOPE_24;
    assert(!probe_config_validate(&bad_cfg)); /* ch 36 not in 2.4 */
    bad_cfg.wifi_band = WIFI_BAND_SCOPE_5;
    assert(probe_config_validate(&bad_cfg));  /* ch 36 valid in US 5 GHz */

    /* Duplicate channel rejected. */
    strncpy(bad_cfg.wifi_channels, "36,36", sizeof bad_cfg.wifi_channels - 1);
    assert(!probe_config_validate(&bad_cfg));

    /* Invalid LoRa ranges rejected. */
    bad_cfg = cfg;
    bad_cfg.lora_freq_hz = 100000000UL;
    assert(!probe_config_validate(&bad_cfg));
    bad_cfg = cfg;
    bad_cfg.lora_sf = 13;
    assert(!probe_config_validate(&bad_cfg));
    bad_cfg = cfg;
    bad_cfg.lora_bw_khz = 100;
    assert(!probe_config_validate(&bad_cfg));
    bad_cfg = cfg;
    bad_cfg.lora_cr = 5;
    assert(!probe_config_validate(&bad_cfg));

    /* Missing NUL terminator rejected. */
    bad_cfg = cfg;
    memset(bad_cfg.wifi_region, 'U', sizeof bad_cfg.wifi_region);
    assert(!probe_config_validate(&bad_cfg));

    printf("probe config codec: defaults, round-trip, CRC32, region/band/channel/LoRa bounds passed\n");
    return 0;
}
