/* Shared external viewport transport; Rev D §5–6. */
#pragma once

#include <cstdint>
#include "model/gnss_model.h"
#include "model/spectrum_model.h"
#include "model/lora_model.h"
#include "model/drive_trail.h"
#include "model/bt_model.h"
#include "model/contacts_model.h"
#include "model/zig_model.h"
#include "ui/external_system_view.h"

namespace ui {
/* Hold the panel in reset while SD and settings initialize at boot. */
void holdExternalGpsDisplayReset();
/* Call after storage::begin() initializes the shared SPI bus. */
bool beginExternalGpsDisplay();
/* Transfers at most one 24-pixel strip per call; samples GPS once per frame. */
void tickExternalGpsDisplay(const model::GnssModel &gps, uint32_t bytes,
                            uint32_t checksum_failures, uint32_t now_ms,
                            const model::SpectrumModel *spectrum = nullptr, size_t cursor = 0,
                            const model::LoraModel *lora = nullptr, size_t lora_cursor = 0,
                            const model::DriveTrail *drive = nullptr,
                            const model::BtModel *bt = nullptr, size_t bt_cursor = 0,
                            const model::ContactsModel *contacts = nullptr,
                            bool probes = false, size_t contacts_cursor = 0,
                            const model::ZigModel *zig = nullptr, bool pans = false,
                            size_t zig_cursor = 0, const model::SystemStatus *system = nullptr);
}
