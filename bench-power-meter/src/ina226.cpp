/*
 * ina226.cpp — see ina226.h. Register map: TI INA226 datasheet.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ina226.h"

namespace {
constexpr uint8_t kRegConfig = 0x00, kRegShunt = 0x01, kRegBus = 0x02, kRegMaskEn = 0x06,
                  kRegAlertLim = 0x07, kRegMfgId = 0xFE, kRegDieId = 0xFF;
constexpr uint16_t kMfgTi = 0x5449, kDieId = 0x2260;   /* die: upper 12 bits */
/* AVG=1, VBUSCT=VSHCT=140 µs, continuous shunt+bus. */
constexpr uint16_t kConfig = 0x4007;
constexpr uint16_t kMaskBul = 1u << 12, kMaskAff = 1u << 4, kMaskLen = 1u << 0;
}  // namespace

bool Ina226::identify()
{
    uint32_t mfg = 0, die = 0;
    return bus_.read(kRegMfgId, 2, mfg) && bus_.read(kRegDieId, 2, die) &&
           mfg == kMfgTi && (die & 0xFFF0) == kDieId;
}

bool Ina226::configure()
{
    return bus_.write16(kRegConfig, kConfig);
}

bool Ina226::setSagAlert(uint32_t mv)
{
    if (mv == 0) return bus_.write16(kRegMaskEn, 0);
    return bus_.write16(kRegAlertLim, meter::ina226AlertLimit(mv)) &&
           bus_.write16(kRegMaskEn, kMaskBul | kMaskLen);
}

bool Ina226::readSag(bool &sag)
{
    uint32_t v = 0;
    if (!bus_.read(kRegMaskEn, 2, v)) return false;
    sag = (v & kMaskAff) != 0;
    return true;
}

bool Ina226::readShunt(int32_t &raw)
{
    uint32_t v = 0;
    if (!bus_.read(kRegShunt, 2, v)) return false;
    raw = meter::ina226Shunt(static_cast<uint16_t>(v));
    return true;
}

bool Ina226::readBusMv(int32_t &mv)
{
    uint32_t v = 0;
    if (!bus_.read(kRegBus, 2, v)) return false;
    mv = meter::ina226BusMv(static_cast<uint16_t>(v));
    return true;
}
