/*
 * ina_bus.h — big-endian register I/O for TI current monitors over Arduino
 * Wire. Tracks the register pointer so the hot path can re-read without
 * re-sending it.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <Wire.h>
#include <cstdint>

class InaBus {
public:
    explicit InaBus(TwoWire &wire, uint8_t addr = 0x40) : wire_(wire), addr_(addr) {}

    /* `bytes` is 2 or 3. */
    bool read(uint8_t reg, uint8_t bytes, uint32_t &val);
    bool write16(uint8_t reg, uint16_t val);

    uint32_t errors() const { return errors_; }
    void clearErrors() { errors_ = 0; }

private:
    TwoWire &wire_;
    uint8_t addr_;
    int pointer_ = -1;
    uint32_t errors_ = 0;
};
