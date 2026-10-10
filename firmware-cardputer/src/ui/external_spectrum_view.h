/* Packet-count companion viewport; brand guide external Packet Monitor. */
#pragma once
#include <M5GFX.h>
#include "model/spectrum_model.h"
namespace ui {
void drawExternalSpectrumStrip(lgfx::LGFX_Sprite &strip, int top,
                              const model::SpectrumModel &model, size_t cursor, uint32_t now);
}
