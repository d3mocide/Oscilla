/* Rendered reference: master guide Drive / Session Log. */
#pragma once
#include <M5GFX.h>
#include "model/drive_trail.h"
#include "storage/wardrive_logger.h"
namespace ui {
void drawExternalDriveStrip(lgfx::LGFX_Sprite &strip,int top,const model::GnssModel &gps,
                            const model::DriveTrail &trail,const storage::WardriveLogStats &log,uint32_t now);
}
