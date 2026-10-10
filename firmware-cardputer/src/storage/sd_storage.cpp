/*
 * sd_storage.cpp — see sd_storage.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/sd_storage.h"

#include <SD.h>
#include <SPI.h>

namespace storage {

namespace {
constexpr int kSckPin = 40;
constexpr int kMisoPin = 39;
constexpr int kMosiPin = 14;
constexpr int kCsPin = 12;
#if defined(OSCILLA_SD_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
constexpr uint32_t kSpiHz = 8000000; /* ILI9341 serial-write minimum period: 100 ns. */
#else
constexpr uint32_t kSpiHz = 25000000;
#endif

SemaphoreHandle_t g_lock;
bool g_ready = false;
}  // namespace

bool begin()
{
    g_lock = xSemaphoreCreateMutex();
    if (!g_lock) return false;

    SPI.begin(kSckPin, kMisoPin, kMosiPin, kCsPin);
#ifdef OSCILLA_NO_CS_DISPLAY_BENCH
    return false; /* No chip select on bench panel: never send SD traffic. */
#else
    g_ready = SD.begin(kCsPin, SPI, kSpiHz);
    return g_ready;
#endif
}

bool ready() { return g_ready; }

void markFault() { g_ready = false; }

bool remount(uint32_t timeout_ms)
{
#ifdef OSCILLA_NO_CS_DISPLAY_BENCH
    (void)timeout_ms;
    return false;
#else
    if (!g_lock || !lock(timeout_ms)) return false;

    g_ready = false;
    SD.end();
    g_ready = SD.begin(kCsPin, SPI, kSpiHz);

    unlock();
    return g_ready;
#endif
}

bool lock(uint32_t timeout_ms) { return g_lock && xSemaphoreTake(g_lock, pdMS_TO_TICKS(timeout_ms)) == pdTRUE; }
void unlock() { xSemaphoreGive(g_lock); }

}  // namespace storage
