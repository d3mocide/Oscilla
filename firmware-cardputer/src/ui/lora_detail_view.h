#pragma once
#include "model/inspection_state.h"
#include "ui/chrome.h"
namespace ui {
void drawLoraDetailView(const model::PacketInspection &packet,const ChromeState &chrome,const char *parent="PACKETS");
}
