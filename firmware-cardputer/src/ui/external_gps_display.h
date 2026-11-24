/* External companion renderer; ST7789 transport lives in external_tft. */
#pragma once

#include <cstdint>
#include "model/gnss_model.h"
#include "model/session_review.h"
#include "model/inspection_state.h"
#include "model/system_resources.h"
#include "model/contact_inspection.h"
#include "model/device_inspection.h"
#include "model/spectrum_model.h"
#include "model/scan_model.h"
#include "model/lora_model.h"
#include "model/ook_model.h"
#include "model/lora_sweep_model.h"
#include "model/signal_hunt.h"
#include "model/drive_trail.h"
#include "model/bt_model.h"
#include "model/contacts_model.h"
#include "model/zig_model.h"
#include "model/deauth_model.h"
#include "model/anti_surveillance_model.h"
#include "ui/external_system_view.h"
#include "ui/external_wifi_view.h"
#include "ui/settings_state.h"

namespace ui {
struct ExternalRenderStats {
    uint32_t frames = 0, last_frame_us = 0, max_frame_us = 0;
};
const ExternalRenderStats &externalRenderStats();
struct ExternalCardState {
    const model::SessionReview *sessions=nullptr;
    const model::WifiInspection *wifi_detail=nullptr;
    const model::ScanModel *wifi_context=nullptr;
    const model::BtInspection *bt_detail=nullptr;
    const model::ContactInspection *contact_detail = nullptr;
    const model::SystemResources *system_resources = nullptr;
    const model::ContactsModel *observed_contacts = nullptr;
    const model::PacketInspection *packet = nullptr;
    const model::LoraSweepModel *lora_sweep = nullptr;
    const model::SignalHunt *signal_hunt = nullptr;
    bool drive_closing = false;
    bool trace_page = false;
    bool trace_listening = false;
    const model::DeauthModel *deauth = nullptr;
    size_t deauth_cursor = 0;
    const model::AntiSurveillanceModel *anti = nullptr;
    size_t anti_cursor = 0;
    bool anti_starting = false;
    const SettingsViewState *settings = nullptr;
    const model::OokModel *ook = nullptr;
    size_t ook_cursor = 0;
    WifiBaselineBanner wifi_baseline;
};
/* Hold the panel in reset while SD and settings initialize at boot. */
void holdExternalGpsDisplayReset();
/* Call after storage::begin(); sends startup through the native TFT driver. */
bool beginExternalGpsDisplay();
/* Put the external panel to sleep / wake it under the shared SPI lock;
 * skips strip redraws while asleep and forces a full repaint after wake. */
bool setExternalGpsDisplaySleep(bool sleep, uint32_t now_ms);
/* One strip per call, changed 40-pixel spans only; 50/250 ms frame targets.
 * Model samples remain coherent for each ten-strip frame. */
void tickExternalGpsDisplay(const model::GnssModel &gps, uint32_t bytes,
                            uint32_t checksum_failures, uint32_t now_ms,
                            const model::ScanModel *scan = nullptr, size_t scan_cursor = 0,
                            const model::SpectrumModel *spectrum = nullptr, size_t cursor = 0,
                            const model::LoraModel *lora = nullptr, size_t lora_cursor = 0,
                            const model::DriveTrail *drive = nullptr,
                            const model::BtModel *bt = nullptr, size_t bt_cursor = 0,
                            const model::ContactsModel *contacts = nullptr,
                            bool probes = false, size_t contacts_cursor = 0,
                            const model::ZigModel *zig = nullptr, bool pans = false,
                            size_t zig_cursor = 0, const model::SystemStatus *system = nullptr,
                            const ExternalCardState *card = nullptr);
}
