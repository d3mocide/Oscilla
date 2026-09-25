/*
 * oled.h — optional 128x64 status display (SSD1306 or SH1106) on the S3's
 * second I²C controller, drawn from its own task on core 0 so refreshes
 * never stall current sampling on core 1.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "display_text.h"

enum class OledPanel { Ssd1306 = 0, Sh1106 = 1 };

/* Starts the bus and loads the saved panel type. */
void oledBegin(int sda, int scl);
/* Starts the display task if a panel answers at 0x3C or 0x3D. Idempotent;
 * cheap to retry while no panel is fitted. */
bool oledAttach();
/* Saved to NVS; used from the next boot. Switching live hung the meter
 * on the bench, so the caller restarts instead. */
bool oledSavePanel(OledPanel p);
const char *oledPanelName();
/* Frames sent since boot: proves the task is drawing. */
uint32_t oledFrames();
/* Comma-separated 7-bit addresses answering on the display bus, with its
 * pins as wired and then swapped. Only while no panel is attached. */
void oledScan(char *as_wired, char *swapped, size_t cap);
/* Address found, or 0. */
uint8_t oledAddress();
/* Hand the task a new snapshot; cheap, safe from the sampling loop. */
void oledPublish(const meter::DisplaySnapshot &s);
