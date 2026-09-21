/* zig_view.h — passive 802.15.4 PAN/node renderer. */
#pragma once
#include <cstddef>
#include <cstdint>
#include "model/zig_model.h"
#include "ui/chrome.h"
namespace ui {
enum class ZigTab : uint8_t { Pans, Nodes };
void drawZigView(const model::ZigModel &zig, ZigTab tab, size_t cursor, const ChromeState &chrome);
}  // namespace ui
