/* External startup and system diagnostics; deck retains all controls. */
#pragma once
#include <M5GFX.h>
#include "model/system_status.h"
namespace ui {
void drawExternalStartupStrip(lgfx::LGFX_Sprite &strip, int top);
void drawExternalSystemStrip(lgfx::LGFX_Sprite &strip, int top,
                            const model::SystemStatus &state, uint32_t now);
}
