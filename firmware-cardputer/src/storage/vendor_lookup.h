/*
 * vendor_lookup.h — offline SD-backed OUI and Bluetooth Company-ID lookups
 * under storage::lock() with fixed RAM LRU caches (WP-12a). Returns empty
 * strings cleanly when /oscilla/oui.bin or /oscilla/bt_mfr.bin is absent.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace storage {

/* Resolves the 3-octet OUI prefix of a MAC ("AA:BB:CC:...") via /oscilla/oui.bin. */
std::string lookupOuiVendor(const std::string &mac);

/* Resolves a Bluetooth SIG 16-bit Company ID hex string ("004c") via /oscilla/bt_mfr.bin. */
std::string lookupBtCompany(const std::string &mfr_hex);

/* Clears cached state and re-probes SD table presence on next lookup. */
void resetVendorLookup();

bool ouiTablePresent();
bool btMfrTablePresent();

}  // namespace storage
