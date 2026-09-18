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
constexpr const char *kBrightnessFlag = "/oscilla/brightness";
/* Mid-scale default (M5GFX 0-255): visible in daylight and indoors alike
 * without having read a saved value yet. Never 0 — see settings.h. */
constexpr uint8_t kBrightnessDefault = 160;
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

uint8_t loadBrightness()
{
    if (!ready()) return kBrightnessDefault;
    if (!lock()) return kBrightnessDefault;
    uint8_t value = kBrightnessDefault;
    File f = SD.open(kBrightnessFlag, FILE_READ);
    if (f) {
        int b = f.read();
        if (b > 0) value = static_cast<uint8_t>(b);   /* 0 is never a saved value, see settings.h */
        f.close();
    }
    unlock();
    return value;
}

void saveBrightness(uint8_t value)
{
    if (!ready()) return;
    if (!lock()) return;
    SD.mkdir(kDir);
    File f = SD.open(kBrightnessFlag, FILE_WRITE);
    if (f) {
        f.write(value);
        f.close();
    }
    unlock();
}

}  // namespace storage
