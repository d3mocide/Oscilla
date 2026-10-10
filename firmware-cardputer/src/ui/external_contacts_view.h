/* Rendered-guide Contacts field: reported relationships, no bearing inference. */
#pragma once
#include <M5GFX.h>
#include "model/contacts_model.h"
namespace ui {
void drawExternalContactsStrip(lgfx::LGFX_Sprite &strip, int top,
    const model::ContactsModel &contacts, bool probes, size_t cursor, uint32_t now);
}
