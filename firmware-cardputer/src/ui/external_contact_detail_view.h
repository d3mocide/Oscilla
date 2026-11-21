#pragma once
#include <M5GFX.h>
#include "model/contact_inspection.h"
namespace ui {
void drawExternalContactDetailStrip(lgfx::LGFX_Sprite &strip,int top,const model::ContactsModel &contacts,
                                   const model::ContactInspection &detail,uint32_t now);
}
