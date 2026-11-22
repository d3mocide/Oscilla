/* Rendered reference: master guide external Observe / Frame List. */
#pragma once
#include <M5GFX.h>
#include "model/lora_model.h"
#include "model/lora_sweep_model.h"
#include "model/inspection_state.h"
namespace ui {
void drawExternalLoraDetailStrip(lgfx::LGFX_Sprite &strip,int top,const model::PacketInspection &packet);
void drawExternalLoraStrip(lgfx::LGFX_Sprite &strip, int top,
                           const model::LoraModel &model, size_t cursor, uint32_t now);
void drawExternalLoraSweepStrip(lgfx::LGFX_Sprite &strip, int top,
                                const model::LoraSweepModel &sweep, uint32_t now);
}
