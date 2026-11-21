/* External Wi-Fi survey companion viewport. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <M5GFX.h>
#include "model/scan_model.h"
#include "model/contacts_model.h"

namespace ui {
struct WifiBaselineBanner {
    bool active = false;
    bool selected_flagged = false;
    bool selected_high = false;
    uint16_t total_alerts = 0;
    const char *selected_label = "";
};
void drawExternalWifiStrip(lgfx::LGFX_Sprite &d, int top,
                           const model::ScanModel &scan, size_t cursor,
                           uint32_t now_ms, bool new_observation = false,
                           const model::ContactsModel *contacts = nullptr,
                           const WifiBaselineBanner *baseline = nullptr);
}
