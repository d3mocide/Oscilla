/*
 * contacts_view.h — the Contacts screen (DESIGN §7.2): live AP<->client map
 * and probe-request SSIDs from the promiscuous sniffer.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "model/contacts_model.h"

namespace ui {

enum class ContactsTab : uint8_t { Clients, Probes };

void drawContactsView(const model::ContactsModel &contacts, ContactsTab tab, size_t cursor,
                      const std::string &notice);

}  // namespace ui
