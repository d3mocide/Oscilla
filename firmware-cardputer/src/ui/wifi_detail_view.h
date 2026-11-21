#pragma once
#include "model/device_inspection.h"
#include "ui/chrome.h"
#include <M5GFX.h>
namespace ui {
void drawWifiDetailView(const model::WifiInspection &,const model::SurveyState &,const model::ContactsModel &,bool,uint32_t,const ChromeState &);
void drawExternalWifiDetailStrip(lgfx::LGFX_Sprite &,int,const model::WifiInspection &,const model::SurveyState &,const model::ContactsModel &,bool,uint32_t);
}
