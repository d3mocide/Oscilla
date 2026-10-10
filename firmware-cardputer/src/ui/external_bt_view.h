/* Rendered-guide BLE field; angular position is device index, not bearing. */
#pragma once
#include <M5GFX.h>
#include "model/bt_model.h"
namespace ui {
void drawExternalBtStrip(lgfx::LGFX_Sprite &strip, int top,
                         const model::BtModel &bt, size_t cursor);
}
