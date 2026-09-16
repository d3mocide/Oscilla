/*
 * settings.cpp — see settings.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/settings.h"

#include <SD.h>

#include "storage/sd_storage.h"

namespace storage {

namespace {
constexpr const char *kDir = "/oscilla";
constexpr const char *kDebugModeFlag = "/oscilla/debug_mode";
}  // namespace

bool loadDebugMode()
{
    if (!ready()) return false;
    if (!lock()) return false;
    bool on = SD.exists(kDebugModeFlag);
    unlock();
    return on;
}

void saveDebugMode(bool on)
{
    if (!ready()) return;
    if (!lock()) return;
    if (on) {
        SD.mkdir(kDir);
        File f = SD.open(kDebugModeFlag, FILE_WRITE);
        if (f) f.close();
    } else {
        SD.remove(kDebugModeFlag);
    }
    unlock();
}

}  // namespace storage
