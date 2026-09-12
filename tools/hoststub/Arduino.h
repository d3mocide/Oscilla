/*
 * Arduino.h — minimal host stub.
 *
 * Not a simulator. It exists so the deck's framework-agnostic layers
 * (DESIGN §7.1: everything from the OCP client down) and its use of the OCP
 * contract can be compiled and tested on a workstation, without an ESP32 or a
 * PlatformIO toolchain. Only the surface the deck actually touches is stubbed;
 * anything missing should be added deliberately, not reflexively.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef OSCILLA_HOSTSTUB_ARDUINO_H
#define OSCILLA_HOSTSTUB_ARDUINO_H

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

#define SERIAL_8N1 0x800001c

class HostSerial {
public:
    void begin(unsigned long baud) { baud_ = baud; }
    void begin(unsigned long baud, uint32_t cfg, int rx, int tx)
    {
        baud_ = baud; (void)cfg; rx_ = rx; tx_ = tx;
    }
    void print(const char *s) { out_ += s; }
    void println(const char *s) { out_ += s; out_ += "\n"; }
    int printf(const char *fmt, ...)
    {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof buf, fmt, ap);
        va_end(ap);
        out_ += buf;
        return n;
    }
    int available() { return 0; }
    int read() { return -1; }

    const std::string &captured() const { return out_; }
    unsigned long baud() const { return baud_; }
    int rx_pin() const { return rx_; }
    int tx_pin() const { return tx_; }

private:
    std::string   out_;
    unsigned long baud_ = 0;
    int           rx_ = -1;
    int           tx_ = -1;
};

inline HostSerial Serial;
inline HostSerial Serial1;
inline HostSerial Serial2;

inline void delay(unsigned long) {}
inline unsigned long millis() { return 0; }

#endif /* OSCILLA_HOSTSTUB_ARDUINO_H */
