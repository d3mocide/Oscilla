/* SD storage and shared TFT bus lock; DESIGN §7.4 / Rev D §5.4. */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace storage {

/* Mounts the internal microSD at boot (Rev D: SCK=40 MOSI=14 MISO=39 CS=12).
 * False if no card is present or mount failed — callers must treat that as
 * "logging unavailable", never retry-loop or block the UI on it. */
bool begin();

bool ready();

/* Mark the mounted card unusable after a verified I/O failure. */
void markFault();

/* Unmounts and mounts the card once. Call only after a confirmed card fault;
 * this is an explicit recovery attempt, not a background retry loop. */
bool remount(uint32_t timeout_ms = 200);

/* Hold only for the shortest bounded operation (a row append, a file open)
 * and never across a redraw, per DESIGN §7.4. Bounded wait, not
 * portMAX_DELAY: a wedged holder must not freeze the whole deck. */
bool lock(uint32_t timeout_ms = 200);
void unlock();

}  // namespace storage
