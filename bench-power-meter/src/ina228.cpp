/*
 * ina228.cpp — see ina228.h. Register map: TI INA228 datasheet.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ina228.h"

namespace {
constexpr uint8_t kRegConfig = 0x00, kRegAdcConfig = 0x01, kRegVshunt = 0x04, kRegVbus = 0x05,
                  kRegDiagAlrt = 0x0B, kRegBuvl = 0x0F, kRegMfgId = 0x3E, kRegDevId = 0x3F;
constexpr uint16_t kMfgTi = 0x5449, kDevId = 0x2280;   /* upper 12 bits; low 4 = revision */
/* ADCRANGE=0, no conversion delay. */
constexpr uint16_t kConfig = 0x0000;
/* MODE=Bh continuous shunt+bus, VBUSCT=50 µs, VSHCT=150 µs, AVG=1. */
constexpr uint16_t kAdcConfig = 0xB000 | (0u << 9) | (2u << 6);
constexpr uint16_t kAlatch = 1u << 15, kBusUl = 1u << 3;
}  // namespace

bool Ina228::identify()
{
    uint32_t mfg = 0, dev = 0;
    return bus_.read(kRegMfgId, 2, mfg) && bus_.read(kRegDevId, 2, dev) &&
           mfg == kMfgTi && (dev & 0xFFF0) == kDevId;
}

bool Ina228::configure()
{
    return bus_.write16(kRegConfig, kConfig) && bus_.write16(kRegAdcConfig, kAdcConfig);
}

bool Ina228::setSagAlert(uint32_t mv)
{
    if (!bus_.write16(kRegBuvl, mv == 0 ? 0 : meter::ina228Buvl(mv))) return false;
    return bus_.write16(kRegDiagAlrt, kAlatch);
}

bool Ina228::readSag(bool &sag)
{
    uint32_t v = 0;
    if (!bus_.read(kRegDiagAlrt, 2, v)) return false;
    sag = (v & kBusUl) != 0;
    return true;
}

bool Ina228::readShunt(int32_t &raw)
{
    uint32_t v = 0;
    if (!bus_.read(kRegVshunt, 3, v)) return false;
    raw = meter::ina228Raw20(v);
    return true;
}

bool Ina228::readBusMv(int32_t &mv)
{
    uint32_t v = 0;
    if (!bus_.read(kRegVbus, 3, v)) return false;
    mv = meter::ina228BusMv(meter::ina228Raw20(v));
    return true;
}
