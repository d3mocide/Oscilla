/* ST7789 transport on the SD's shared SPI2 controller; Rev D §5.4. */
#pragma once
#include <cstdint>

namespace ui {
struct ExternalTftStatus {
    bool initialized = false;
    bool sleeping = false;
    uint32_t rows_sent = 0;
    uint32_t clock_hz = 0;
    uint32_t pixels_sent = 0, max_span_us = 0;
};
/* Deselect and hold reset before storage initializes the shared bus. */
void holdExternalTftReset();
/* Initialize after storage::begin(); owns the shared lock during setup. */
bool beginExternalTft();
/* Put the ST7789 panel into SLPIN/SLPOUT under the shared lock (BLK stays on 3V3, Rev D §5.1). */
bool setExternalTftSleep(bool sleep);
/* Send one native RGB565 row under the shared lock; false means retry. */
bool writeExternalTftRow(int y, const uint16_t *pixels);
/* Send a bounded horizontal span; caller retries after lock contention. */
bool writeExternalTftSpan(int y,int x,int width,const uint16_t *pixels);
/* Driver state and sends only; the panel has no readback connection. */
const ExternalTftStatus &externalTftStatus();
}
