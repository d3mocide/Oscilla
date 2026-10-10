/* Rendered reference: master guide external Observe / Frame List. */
#pragma once
#include <M5GFX.h>
#include "model/lora_model.h"
namespace ui {
void drawExternalLoraStrip(lgfx::LGFX_Sprite &strip, int top,
                           const model::LoraModel &model, size_t cursor, uint32_t now);
}
