/*
 * ina226.cpp — see ina226.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ina226.h"

static constexpr uint16_t kMfgTi = 0x5449;
static constexpr uint16_t kDieIna226 = 0x2260;   /* upper 12 bits; low 4 = revision */
static constexpr uint16_t kMaskBul = 1u << 12, kMaskAff = 1u << 4, kMaskLen = 1u << 0;

bool Ina226::begin(uint16_t config)
{
    uint16_t mfg = 0, die = 0;
    if (!readReg(kRegMfgId, mfg) || !readReg(kRegDieId, die)) return false;
    if (mfg != kMfgTi || (die & 0xFFF0) != kDieIna226) return false;
    return writeReg(kRegConfig, config);
}

bool Ina226::setSagAlert(uint32_t mv)
{
    if (mv == 0) return writeReg(kRegMaskEn, 0);
    if (!writeReg(kRegAlertLim, static_cast<uint16_t>(mv * 4 / 5))) return false;
    return writeReg(kRegMaskEn, kMaskBul | kMaskLen);
}

bool Ina226::readSag(bool &sag)
{
    uint16_t v = 0;
    if (!readReg(kRegMaskEn, v)) return false;
    sag = (v & kMaskAff) != 0;
    return true;
}

bool Ina226::readShunt(int16_t &raw)
{
    uint16_t v = 0;
    bool ok = pointer_ == kRegShunt ? readPointed(v) : readReg(kRegShunt, v);
    if (ok) raw = static_cast<int16_t>(v);
    return ok;
}

bool Ina226::readBus(uint16_t &raw)
{
    return readReg(kRegBus, raw);
}

bool Ina226::readReg(uint8_t reg, uint16_t &val)
{
    wire_.beginTransmission(addr_);
    wire_.write(reg);
    if (wire_.endTransmission() != 0) {
        pointer_ = -1;
        errors_++;
        return false;
    }
    pointer_ = reg;
    return readPointed(val);
}

bool Ina226::writeReg(uint8_t reg, uint16_t val)
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

bool Ina226::readPointed(uint16_t &val)
{
    if (wire_.requestFrom(addr_, static_cast<uint8_t>(2)) != 2) {
        pointer_ = -1;
        errors_++;
        return false;
    }
    uint8_t hi = wire_.read();
    uint8_t lo = wire_.read();
    val = static_cast<uint16_t>(hi << 8 | lo);
    return true;
}
