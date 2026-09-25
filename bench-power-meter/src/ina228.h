/*
 * ina228.h — INA228 as a PowerSensor. 20-bit shunt ADC, 312.5 nV LSB at
 * ADCRANGE=0 (±163.84 mV).
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "ina_decode.h"
#include "power_sensor.h"

class Ina228 : public PowerSensor {
public:
    explicit Ina228(InaBus &bus) : bus_(bus) {}

    const char *name() const override { return "ina228"; }
    bool identify() override;
    bool configure() override;
    bool setSagAlert(uint32_t mv) override;
    bool readSag(bool &sag) override;
    bool readShunt(int32_t &raw) override;
    bool readBusMv(int32_t &mv) override;

    int64_t shuntLsbPv() const override { return meter::kIna228ShuntLsbPv; }
    int32_t fullScale() const override { return meter::kIna228FullScale; }
    const char *setup() const override { return "avg1_vbus50us_vsh150us_cont_range163mV"; }

private:
    InaBus &bus_;
};
