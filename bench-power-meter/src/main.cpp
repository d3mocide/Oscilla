/*
 * main.cpp — Oscilla P6 bench power meter on a XIAO ESP32-S3. Not Oscilla
 * firmware: bench instrumentation, like firmware-cardputer/bench/.
 *
 * INA226 in series with the probe's supply; UART bridge to the probe's OCP
 * link; both multiplexed onto USB (meter_mux.h). Method:
 * docs/hardware/power-budget.md. Wiring and line format: README.md.
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include <Wire.h>
#include <esp_timer.h>

#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "ina226.h"
#include "meter_cmd.h"
#include "meter_mux.h"
#include "meter_window.h"
#include "ocp.h"
#include "probe_power.h"

/* Bench wiring (README). Probe end is Rev D §3: C5 D7/GPIO12 RX, D6/GPIO11 TX.
 * GPIO43/44 avoided: the S3 ROM prints on GPIO43 at every reset. */
static constexpr int kProbeTxPin = 2;   /* D1 -> C5 D7 */
static constexpr int kProbeRxPin = 1;   /* D0 <- C5 D6 */
static constexpr int kSdaPin = 5;       /* D4 */
static constexpr int kSclPin = 6;       /* D5 */
static constexpr uint32_t kI2cHz = 400000;

/* AVG=1, VBUSCT=VSHCT=140 µs, continuous shunt+bus (INA226 config register). */
static constexpr uint16_t kInaConfig = 0x4007;
static constexpr int64_t kBusPeriodUs = 10000;
static constexpr size_t kBufBytes = 1024;
static constexpr const char *kMeterVer = "0.1.0";

namespace {

struct StreamSink : meter::Sink {
    explicit StreamSink(Stream &s) : s_(s) {}
    void write(const uint8_t *p, size_t n) override { s_.write(p, n); }
    Stream &s_;
};

StreamSink g_host(Serial);
StreamSink g_probe(Serial1);
meter::Downlink g_down;
meter::Uplink g_up;
meter::ProbePower g_power;
meter::Window g_win;
Ina226 g_ina(Wire);

bool g_ina_ok = false;
bool g_stream = false;
uint32_t g_window_us = 50000;
uint32_t g_shunt_uohm = 20000;
uint32_t g_sag_mv = 3150;
uint32_t g_lost = 0;
int64_t g_last_bus_us = 0;

int64_t nowUs() { return esp_timer_get_time(); }

void emit(const char *line)
{
    if (!g_down.queue(line)) g_lost++;
}

void emitf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void emitf(const char *fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0 || static_cast<size_t>(n) >= sizeof line) {
        g_lost++;
        return;
    }
    emit(line);
}

void emitInfo()
{
    emitf("#PWR info ver=%s ina=%s probe=%s stream=%d window_ms=%" PRIu32
          " shunt_uohm=%" PRIu32 " sag_mv=%" PRIu32 " config=0x%04X",
          kMeterVer, g_ina_ok ? "ok" : "absent", g_power.up() ? "up" : "down",
          g_stream ? 1 : 0, g_window_us / 1000, g_shunt_uohm, g_sag_mv, kInaConfig);
}

/* UART only while the probe is powered: never back-feed an unpowered C5 (Rev D §9). */
void probeUart(bool on)
{
    if (on) {
        Serial1.setRxBufferSize(kBufBytes);
        Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kProbeRxPin, kProbeTxPin);
    } else {
        Serial1.end();
        pinMode(kProbeTxPin, INPUT);
        pinMode(kProbeRxPin, INPUT);
    }
}

struct Commands : meter::CommandSink {
    void command(const char *line, bool overflow) override
    {
        meter::Cmd c = meter::parseCommand(line, overflow);
        switch (c.kind) {
        case meter::CmdKind::Invalid:
            emitf("#PWR err msg=\"%s\"", c.err);
            return;
        case meter::CmdKind::Info:
            emitInfo();
            return;
        case meter::CmdKind::StreamOn:
            g_stream = true;
            meter::windowReset(g_win, nowUs());
            break;
        case meter::CmdKind::StreamOff:
            g_stream = false;
            break;
        case meter::CmdKind::Window:
            g_window_us = c.value * 1000;
            break;
        case meter::CmdKind::Shunt:
            g_shunt_uohm = c.value;
            break;
        case meter::CmdKind::Sag:
            g_sag_mv = c.value;
            if (g_ina_ok && !g_ina.setSagAlert(g_sag_mv)) {
                emitf("#PWR err msg=\"i2c\"");
                return;
            }
            break;
        case meter::CmdKind::Mark:
            emitf("#PWR mark t_us=%" PRId64 " label=%s", nowUs(), c.label);
            return;
        }
        emitf("#PWR ok cmd=\"%s\"", line);   /* validated: label charset only */
    }
} g_cmds;

void pumpHost()
{
    static uint8_t buf[kBufBytes];
    int avail = Serial.available();
    if (avail <= 0) return;
    size_t n = Serial.readBytes(buf, min(static_cast<size_t>(avail), kBufBytes));
    g_up.feed(buf, n, g_power.up() ? &g_probe : nullptr, g_cmds);
}

void pumpProbe()
{
    static uint8_t buf[kBufBytes];
    if (!g_power.up()) return;
    int avail = Serial1.available();
    if (avail <= 0) return;
    size_t n = Serial1.readBytes(buf, min(static_cast<size_t>(avail), kBufBytes));
    g_down.forward(buf, n, nowUs(), g_host);
}

void sampleBus(int64_t now)
{
    uint16_t raw = 0;
    int32_t mv = g_ina.readBus(raw) ? meter::busRawToMv(raw) : -1;
    if (mv >= 0) meter::windowAddBus(g_win, mv);

    switch (g_power.update(mv, now)) {
    case meter::ProbePower::Change::Up:
        probeUart(true);
        if (g_stream) emitf("#PWR probe t_us=%" PRId64 " state=up vbus_mv=%" PRId32, now, mv);
        break;
    case meter::ProbePower::Change::Down:
        probeUart(false);
        g_down.probeGone(g_host);
        if (g_stream) emitf("#PWR probe t_us=%" PRId64 " state=down vbus_mv=%" PRId32, now, mv);
        break;
    case meter::ProbePower::Change::None:
        break;
    }
}

void closeWindow(int64_t now)
{
    /* Previous line not yet out: keep accumulating; t0/t1 make it self-describing. */
    if (!g_down.pendingEmpty()) return;
    bool sag = false;
    if (g_ina_ok && g_sag_mv > 0 && !g_ina.readSag(sag)) sag = false;
    meter::WindowFlags f{sag, g_down.cuts(), g_up.dropped(), g_ina.errors(), g_lost};
    char line[320];
    if (meter::formatWindow(line, sizeof line, g_win, now, g_shunt_uohm, f) < 0) g_lost++;
    else emit(line);
    meter::windowReset(g_win, now);
}

}  // namespace

void setup()
{
    Serial.setRxBufferSize(kBufBytes);
    Serial.setTxBufferSize(kBufBytes);
    Serial.begin(OCP_BAUD_DEFAULT);
    Serial.setTxTimeoutMs(0);   /* host not reading: drop, never block */

    probeUart(false);
    Wire.begin(kSdaPin, kSclPin, kI2cHz);
    Wire.setTimeOut(5);   /* ms: a wedged bus faults, never hangs */
    g_ina_ok = g_ina.begin(kInaConfig) && g_ina.setSagAlert(g_sag_mv);
    meter::windowReset(g_win, nowUs());
}

/* loopTask runs on core 1 with no task-WDT subscription; USB is serviced
 * by its own task, so this tight poll starves nothing. */
void loop()
{
    int64_t now = nowUs();

    pumpHost();
    pumpProbe();

    if (g_ina_ok) {
        int16_t raw = 0;
        if (g_ina.readShunt(raw)) meter::windowAddShunt(g_win, raw);
        if (now - g_last_bus_us >= kBusPeriodUs) {
            g_last_bus_us = now;
            sampleBus(now);
        }
    }

    if (now - g_win.t0_us >= static_cast<int64_t>(g_window_us)) {
        if (g_stream) closeWindow(now);
        else meter::windowReset(g_win, now);
    }

    g_down.service(nowUs(), g_host);
}
