/* zig_radio.h — receive-only ESP-IDF 802.15.4 adapter (D-17). */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct { uint8_t data[128], len, ch, lqi; int8_t rssi; uint32_t ts_ms; } zig_rx_t;

esp_err_t zig_radio_init(void);
esp_err_t zig_radio_start(uint8_t channel);
void zig_radio_stop(void);
void zig_radio_rearm(void);
bool zig_radio_next(zig_rx_t *out, uint32_t wait_ms);
esp_err_t zig_radio_set_channel(uint8_t channel);
uint16_t zig_radio_dropped(void);
