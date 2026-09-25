/*
 * ina226.h — INA226 as a PowerSensor. 16-bit shunt ADC, 2.5 µV LSB.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "ina_decode.h"
#include "power_sensor.h"

class Ina226 : public PowerSensor {
public:
    explicit Ina226(InaBus &bus) : bus_(bus) {}

    const char *name() const override { return "ina226"; }
    bool identify() override;
    bool configure() override;
    bool setSagAlert(uint32_t mv) override;
    bool readSag(bool &sag) override;
    bool readShunt(int32_t &raw) override;
    bool readBusMv(int32_t &mv) override;

    int64_t shuntLsbPv() const override { return meter::kIna226ShuntLsbPv; }
    int32_t fullScale() const override { return meter::kIna226FullScale; }
    const char *setup() const override { return "avg1_vbus140us_vsh140us_cont"; }

private:
    InaBus &bus_;
};
