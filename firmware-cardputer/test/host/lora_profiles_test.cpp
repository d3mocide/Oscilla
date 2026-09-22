/* lora_profiles_test.cpp — pins the sourced preset values in
 * model/lora_profiles.h so a typo or an "improvement" without a source
 * can't silently drift from what LoRaTrace-RX's channel_plans.h verified
 * against upstream firmware. */

#include <cstdio>
#include <cstring>

#include "model/lora_profiles.h"

namespace {
int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}
}  // namespace

int main()
{
    check(model::kLoraProfileCount == 2, "exactly the two source-backed profiles exist");

    const auto &meshcore = model::kLoraProfiles[0];
    check(std::strcmp(meshcore.token, "meshcore_us_ca") == 0 &&
          meshcore.freq_hz == 910525000 && meshcore.sf == 7 && meshcore.bw_khz == 62 &&
          meshcore.cr == 1 && meshcore.sync_word == 0x12,
          "MeshCore US/CA matches the already hardware-qualified bench values");

    const auto &meshtastic = model::kLoraProfiles[1];
    check(std::strcmp(meshtastic.token, "meshtastic_us_longfast") == 0 &&
          meshtastic.freq_hz == 906875000 && meshtastic.sf == 11 && meshtastic.bw_khz == 250 &&
          meshtastic.cr == 1 && meshtastic.sync_word == 0x2B,
          "Meshtastic US LongFast matches LoRaTrace-RX's sourced values");

    check(meshcore.sync_word != meshtastic.sync_word,
          "distinct sync words: a shared value would make both profiles filter identically");

    std::printf("\n%s\n", g_fail ? "lora profiles test FAILED" : "lora profiles test OK");
    return g_fail ? 1 : 0;
}
