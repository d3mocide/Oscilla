/*
 * ina_decode.h — register encodings for the INA226 and INA228 (TI
 * datasheets). Pure arithmetic so it can be host-tested; the I²C drivers
 * only move bytes.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace meter {

/* Shunt LSB in picovolts, so µA = raw * lsb_pv / shunt_uohm. */
constexpr int64_t kIna226ShuntLsbPv = 2500000;   /* 2.5 µV */
constexpr int32_t kIna226FullScale = 32767;
constexpr int64_t kIna228ShuntLsbPv = 312500;    /* 312.5 nV, ADCRANGE=0 (±163.84 mV) */
constexpr int32_t kIna228FullScale = 524287;

inline int32_t ina226Shunt(uint16_t reg) { return static_cast<int16_t>(reg); }
inline int32_t ina226BusMv(uint16_t reg) { return static_cast<int32_t>(reg) * 5 / 4; }   /* 1.25 mV */
inline uint16_t ina226AlertLimit(uint32_t mv) { return static_cast<uint16_t>(mv * 4 / 5); }

/* 24-bit register, value in bits 23:4, two's complement. */
inline int32_t ina228Raw20(uint32_t reg24)
{
    return static_cast<int32_t>(reg24 << 8) >> 12;
}
inline int32_t ina228BusMv(int32_t raw) { return raw * 25 / 128; }   /* 195.3125 µV */
inline uint16_t ina228Buvl(uint32_t mv) { return static_cast<uint16_t>(mv * 8 / 25); }   /* 3.125 mV */

}  // namespace meter
