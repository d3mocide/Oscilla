/* zig_view.h — passive 802.15.4 PAN/node renderer. */
#pragma once
#include <cstddef>
#include "model/zig_model.h"
#include "ui/chrome.h"
namespace ui { void drawZigView(const model::ZigModel &zig, size_t cursor, const ChromeState &chrome); }
