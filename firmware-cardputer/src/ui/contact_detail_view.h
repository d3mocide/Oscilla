#pragma once
#include "model/contact_inspection.h"
#include "ui/chrome.h"
namespace ui {
void drawContactDetailView(const model::ContactInspection &detail,bool live,uint32_t now,const ChromeState &chrome);
}
