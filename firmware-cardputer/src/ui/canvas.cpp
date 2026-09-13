/*
 * canvas.cpp — see canvas.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/canvas.h"

namespace ui {

namespace {
M5Canvas g_canvas(&M5Cardputer.Display);
}  // namespace

void initCanvas()
{
    g_canvas.setColorDepth(16);   /* matches the panel: named TFT_* colours stay exact */
    g_canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    g_canvas.setTextSize(1);
}

M5Canvas &canvas() { return g_canvas; }

void present() { g_canvas.pushSprite(0, 0); }

}  // namespace ui
