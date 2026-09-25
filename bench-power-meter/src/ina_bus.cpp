/*
 * ina_bus.cpp — see ina_bus.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ina_bus.h"

bool InaBus::read(uint8_t reg, uint8_t bytes, uint32_t &val)
{
    if (pointer_ != reg) {
        wire_.beginTransmission(addr_);
        wire_.write(reg);
        if (wire_.endTransmission() != 0) {
            pointer_ = -1;
            errors_++;
            return false;
        }
        pointer_ = reg;
    }
    if (wire_.requestFrom(addr_, bytes) != bytes) {
        pointer_ = -1;
        errors_++;
        return false;
    }
    uint32_t v = 0;
    for (uint8_t i = 0; i < bytes; i++) v = v << 8 | static_cast<uint8_t>(wire_.read());
    val = v;
    return true;
}

bool InaBus::write16(uint8_t reg, uint16_t val)
{
    wire_.beginTransmission(addr_);
    wire_.write(reg);
    wire_.write(static_cast<uint8_t>(val >> 8));
    wire_.write(static_cast<uint8_t>(val & 0xFF));
    bool ok = wire_.endTransmission() == 0;
    pointer_ = ok ? reg : -1;
    if (!ok) errors_++;
    return ok;
}
