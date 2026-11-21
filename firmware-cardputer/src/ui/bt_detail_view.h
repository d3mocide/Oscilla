#pragma once
#include "model/device_inspection.h"
#include "ui/chrome.h"
#include <M5GFX.h>
namespace ui {
void drawBtDetailView(const model::BtInspection &,const model::BtModel &,uint32_t,const ChromeState &);
void drawExternalBtDetailStrip(lgfx::LGFX_Sprite &,int,const model::BtInspection &,const model::BtModel &,uint32_t);
}
