/*
 * settings.cpp — see settings.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/settings.h"

#include <SD.h>
#include <cstdio>
#include <cstdlib>

#include "storage/sd_storage.h"
#include "storage/sd_activity.h"

namespace storage {

namespace {
constexpr const char *kDir = "/oscilla";
constexpr const char *kDebugModeFlag = "/oscilla/debug_mode";
constexpr const char *kBrightnessFlag = "/oscilla/brightness";
constexpr const char *kWifiRegionFlag = "/oscilla/wifi_region";
constexpr const char *kWifiBandFlag = "/oscilla/wifi_band";
constexpr const char *kIdleTimeoutFlag = "/oscilla/idle_timeout";
constexpr const char *kLoraProfileFlag = "/oscilla/lora_profile";
constexpr const char *kLoraProfilesCsv = "/oscilla/lora_profiles.csv";
constexpr size_t kMaxLoraProfilesCsvBytes = 4096;
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
        sdActivity().record(1, f.write(value), millis());
        f.close();
    }
    unlock();
}

std::string loadWifiRegion()
{
    if (!ready()) return {};
    if (!lock()) return {};
    std::string code;
    if (SD.exists(kWifiRegionFlag)) {
        File f = SD.open(kWifiRegionFlag, FILE_READ);
        char buf[4] = {};
        if (f) {
            size_t got = f.read(reinterpret_cast<uint8_t *>(buf), sizeof buf - 1);
            code.assign(buf, got);
            f.close();
        }
    }
    unlock();
    return code;
}

void saveWifiRegion(const std::string &code)
{
    if (!ready()) return;
    if (!lock()) return;
    SD.mkdir(kDir);
    File f = SD.open(kWifiRegionFlag, FILE_WRITE);
    if (f) {
        sdActivity().record(code.size(), f.print(code.c_str()), millis());
        f.close();
    }
    unlock();
}

std::string loadWifiBand()
{
    if (!ready()) return {};
    if (!lock()) return {};
    std::string code;
    if (SD.exists(kWifiBandFlag)) {
        File f = SD.open(kWifiBandFlag, FILE_READ);
        char buf[8] = {};
        if (f) {
            size_t got = f.read(reinterpret_cast<uint8_t *>(buf), sizeof buf - 1);
            code.assign(buf, got);
            f.close();
        }
    }
    unlock();
    return code;
}

void saveWifiBand(const std::string &code)
{
    if (!ready()) return;
    if (!lock()) return;
    SD.mkdir(kDir);
    File f = SD.open(kWifiBandFlag, FILE_WRITE);
    if (f) {
        sdActivity().record(code.size(), f.print(code.c_str()), millis());
        f.close();
    }
    unlock();
}

uint16_t loadIdleTimeoutS()
{
    if (!ready()) return 0;
    if (!lock()) return 0;
    uint16_t value = 0;
    if (SD.exists(kIdleTimeoutFlag)) {
        File f = SD.open(kIdleTimeoutFlag, FILE_READ);
        char buf[12] = {};
        if (f) {
            size_t got = f.read(reinterpret_cast<uint8_t *>(buf), sizeof buf - 1);
            buf[got] = '\0';
            char *end = nullptr;
            unsigned long parsed = std::strtoul(buf, &end, 10);
            if (end && end != buf && parsed <= 65535UL) value = static_cast<uint16_t>(parsed);
            f.close();
        }
    }
    unlock();
    return value;
}

void saveIdleTimeoutS(uint16_t seconds)
{
    if (!ready()) return;
    if (!lock()) return;
    SD.mkdir(kDir);
    File f = SD.open(kIdleTimeoutFlag, FILE_WRITE);
    if (f) {
        char buf[12];
        int len = std::snprintf(buf, sizeof buf, "%u", static_cast<unsigned>(seconds));
        if (len > 0) sdActivity().record(static_cast<size_t>(len), f.print(buf), millis());
        f.close();
    }
    unlock();
}

std::string loadLoraProfile()
{
    if (!ready()) return {};
    if (!lock()) return {};
    std::string token;
    if (SD.exists(kLoraProfileFlag)) {
        File f = SD.open(kLoraProfileFlag, FILE_READ);
        char buf[32] = {};
        if (f) {
            size_t got = f.read(reinterpret_cast<uint8_t *>(buf), sizeof buf - 1);
            token.assign(buf, got);
            f.close();
        }
    }
    unlock();
    return token;
}

void saveLoraProfile(const std::string &token)
{
    if (!ready()) return;
    if (!lock()) return;
    SD.mkdir(kDir);
    File f = SD.open(kLoraProfileFlag, FILE_WRITE);
    if (f) {
        sdActivity().record(token.size(), f.print(token.c_str()), millis());
        f.close();
    }
    unlock();
}

std::string loadLoraProfilesCsv()
{
    if (!ready()) return {};
    if (!lock()) return {};
    std::string csv;
    if (SD.exists(kLoraProfilesCsv)) {
        File f = SD.open(kLoraProfilesCsv, FILE_READ);
        if (f && !f.isDirectory()) {
            const size_t sz = f.size();
            const size_t cap = sz < kMaxLoraProfilesCsvBytes ? sz : kMaxLoraProfilesCsvBytes;
            csv.resize(cap);
            if (cap > 0) {
                const size_t got = f.read(reinterpret_cast<uint8_t *>(&csv[0]), cap);
                csv.resize(got);
            }
            f.close();
        } else if (f) {
            f.close();
        }
    }
    unlock();
    return csv;
}

}  // namespace storage
