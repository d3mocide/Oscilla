#pragma once
#include "model/session_review.h"
#include "ui/chrome.h"
#include <M5GFX.h>
namespace ui {
void drawSessionView(const model::SessionReview &review, const ChromeState &chrome);
void drawExternalSessionStrip(lgfx::LGFX_Sprite &sprite, int top, const model::SessionReview &review);
} // namespace ui
