/*
 * lora_profiles.h — named LoRa receive-parameter presets for lora_config.
 *
 * Values (frequency/SF/BW/CR and sync word) are cross-checked against
 * LoRaTrace-RX's src/channel_plans.h, which cites them against upstream
 * firmware source (meshtastic/firmware RadioLibInterface.h, meshcore-dev/
 * MeshCore CustomSX1262.h) — not re-derived here. A named profile records
 * radio parameters only; it is not a protocol assertion or decoder
 * selection (docs/lora-session-integrity.md's LSI-1 non-goals). Adding a
 * profile here means it has a real source, the same standard as these two.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace model {

struct LoraProfileDef {
    const char *token;   /* wire/manifest identifier, e.g. session provenance */
    const char *label;   /* short on-screen name, fits the Sub-GHz card's width */
    uint32_t freq_hz;
    int sf;
    int bw_khz;
    int cr;               /* 1..4 -> 4/5..4/8 */
    uint8_t sync_word;
};

/* MeshCore — US/Canada "Recommended" preset, post-Oct-2025 narrow-BW
 * migration (already hardware-qualified, see docs/hardware/lora-harness.md).
 * Meshtastic — US LongFast default, slot 20 of 104: one slot out of a
 * 104-slot hash space, not full protocol coverage (non-default channel
 * names land on other slots this profile does not cover). */
constexpr LoraProfileDef kLoraProfiles[] = {
    {"meshcore_us_ca", "MCORE", 910525000, 7, 62, 1, 0x12},
    {"meshtastic_us_longfast", "MTASTIC", 906875000, 11, 250, 1, 0x2B},
};
constexpr size_t kLoraProfileCount = sizeof(kLoraProfiles) / sizeof(kLoraProfiles[0]);

}  // namespace model
