/*
 * power_sensor.h — what main.cpp needs from a current monitor, and
 * detection of which one is fitted (INA226 or INA228, by ID register).
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

#include "ina_bus.h"

class PowerSensor {
public:
    virtual ~PowerSensor() = default;

    virtual const char *name() const = 0;
    /* Checks IDs only; no writes. */
    virtual bool identify() = 0;
    virtual bool configure() = 0;
    /* Latching bus-under-voltage alert at `mv`; 0 disables it. */
    virtual bool setSagAlert(uint32_t mv) = 0;
    /* Reads the latched flag; reading clears it. */
    virtual bool readSag(bool &sag) = 0;
    virtual bool readShunt(int32_t &raw) = 0;
    virtual bool readBusMv(int32_t &mv) = 0;

    virtual int64_t shuntLsbPv() const = 0;
    virtual int32_t fullScale() const = 0;
    /* Human-readable ADC setup for #info. */
    virtual const char *setup() const = 0;
};

/* The fitted sensor, configured; nullptr if none answers. */
PowerSensor *detectSensor(InaBus &bus);
