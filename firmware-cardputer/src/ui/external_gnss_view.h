/* GNSS fix instrument; source freshness is never inferred from UART bytes. */
#pragma once
#include <M5GFX.h>
#include "model/gnss_model.h"
namespace ui {
void drawExternalGnssStrip(lgfx::LGFX_Sprite &,int,const model::GnssModel &,uint32_t now);
}
