/* Passive MAC inventory; guide topology is a future concept. */
#pragma once
#include <M5GFX.h>
#include "model/zig_model.h"
namespace ui {
void drawExternalZigStrip(lgfx::LGFX_Sprite &strip, int top,
                         const model::ZigModel &model, bool pans, size_t cursor);
}
