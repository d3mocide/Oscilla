/*
 * canvas.h — the deck's single off-screen draw target.
 *
 * Every drawXView() renders onto this M5Canvas sprite instead of
 * M5Cardputer.Display directly; DeckApp::draw() blits it to the real panel
 * in one shot with present(). This is what actually removes the visible
 * flicker: a fillScreen()+redraw straight to the panel is visible mid
 * SPI-transfer every redraw; the same sequence done to a sprite in RAM is
 * invisible until pushSprite() lands it atomically.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <M5Cardputer.h>

namespace ui {

/* Call once from setup(), after M5Cardputer.begin(). */
void initCanvas();

/* The shared draw target. Same drawing API as M5Cardputer.Display
 * (LovyanGFX): fillScreen, setCursor, printf, fillRect, setTextColor, ... */
M5Canvas &canvas();

/* Blit the canvas to the real panel. Call once per frame, after drawing. */
void present();

}  // namespace ui
