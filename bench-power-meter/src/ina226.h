/*
 * ina226.h — minimal INA226 driver over Arduino Wire. Reads raw registers
 * only; scaling lives in meter_window.h so it can be host-tested.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <Wire.h>
#include <cstdint>

class Ina226 {
public:
    static constexpr uint8_t kRegConfig = 0x00, kRegShunt = 0x01, kRegBus = 0x02,
                             kRegMaskEn = 0x06, kRegAlertLim = 0x07,
                             kRegMfgId = 0xFE, kRegDieId = 0xFF;

    explicit Ina226(TwoWire &wire, uint8_t addr = 0x40) : wire_(wire), addr_(addr) {}

    /* Checks the TI/INA226 IDs, then writes `config`. */
    bool begin(uint16_t config);
    /* Latching bus-under-voltage alert at `mv`; 0 disables it. */
    bool setSagAlert(uint32_t mv);
    /* Reads the latched alert flag; reading clears it. */
    bool readSag(bool &sag);

    /* Hot path: re-reads the shunt register without re-sending the pointer. */
    bool readShunt(int16_t &raw);
    bool readBus(uint16_t &raw);

    uint32_t errors() const { return errors_; }

private:
    bool readReg(uint8_t reg, uint16_t &val);
    bool writeReg(uint8_t reg, uint16_t val);
    bool readPointed(uint16_t &val);

    TwoWire &wire_;
    uint8_t addr_;
    int pointer_ = -1;
    uint32_t errors_ = 0;
};
