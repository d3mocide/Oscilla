/*
 * power_sensor.cpp — see power_sensor.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "power_sensor.h"

#include "ina226.h"
#include "ina228.h"

PowerSensor *detectSensor(InaBus &bus)
{
    static Ina226 ina226(bus);
    static Ina228 ina228(bus);
    PowerSensor *const candidates[] = {&ina228, &ina226};

    for (PowerSensor *s : candidates) {
        if (!s->identify()) continue;
        bus.clearErrors();   /* the other chip's ID reads may have NACKed */
        return s->configure() ? s : nullptr;
    }
    /* Failed probes stay counted: #info i2c_err shows a sick bus. */
    return nullptr;
}
