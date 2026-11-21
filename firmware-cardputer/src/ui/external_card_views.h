/* External companion renderers for deck cards without dedicated modules. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <M5GFX.h>
#include "model/scan_model.h"
#include "model/deauth_model.h"
#include "model/anti_surveillance_model.h"
#include "ui/settings_state.h"
namespace ui {
void drawExternalDeauthStrip(lgfx::LGFX_Sprite &,int,const model::DeauthModel &,size_t,bool,uint32_t);
void drawExternalAntiStrip(lgfx::LGFX_Sprite &,int,const model::AntiSurveillanceModel &,size_t,bool,uint32_t);
void drawExternalSettingsStrip(lgfx::LGFX_Sprite &,int,const SettingsViewState &);
}
