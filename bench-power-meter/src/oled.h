/*
 * oled.h — optional SSD1306 128x64 status display on the S3's second I²C
 * controller, drawn from its own task on core 0 so refreshes never stall
 * current sampling on core 1.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "display_text.h"

void oledBegin(int sda, int scl);
/* Starts the display task if a panel answers at 0x3C or 0x3D. Idempotent;
 * cheap to retry while no panel is fitted. */
bool oledAttach();
/* Address found, or 0. */
uint8_t oledAddress();
/* Hand the task a new snapshot; cheap, safe from the sampling loop. */
void oledPublish(const meter::DisplaySnapshot &s);
