/* 433 MHz OOK transmitter lanes with a ten-minute heard-when timeline; Observatory v2.
 * Ticks come only from packets the deck still holds (the newest 32), so an
 * older press simply isn't drawn: nothing is interpolated or invented. */
#include "ui/external_ook_view.h"
#include "ui/observatory_chrome.h"
#include <cstdio>
#include <cstdlib>

namespace ui {
namespace {

constexpr uint32_t kWindowMs = 600000;   /* ten minutes */
constexpr int kLanes = 6, kLaneTop = 44, kLaneStep = 15;
constexpr int kTimeX0 = 122, kTimeX1 = 266;

std::string identity(const model::OokPacket &p)
{
    char s[24];
    if (p.decoder == model::OokDecoder::LaCrosse) std::snprintf(s, sizeof s, "LAX %u/%u", p.id, p.ch);
    else std::snprintf(s, sizeof s, "PRN %s", p.hex.c_str());
    return observatory::obsUpper(s);
}

std::string age(uint32_t now, uint32_t then)
{
    const uint32_t s = now >= then ? (now - then) / 1000u : 0u;
    char v[12];
    if (s < 100) std::snprintf(v, sizeof v, "%lus", (unsigned long)s);
    else if (s < 6000) std::snprintf(v, sizeof v, "%lum", (unsigned long)(s / 60));
    else std::snprintf(v, sizeof v, "%luh", (unsigned long)(s / 3600));
    return v;
}

/* Same thresholds as list_row.h's rssiColour (that header pulls in M5Cardputer). */
uint16_t rssiTone(int rssi)
{
    return rssi >= -55 ? kFieldGreen : rssi >= -70 ? kCalibrationYellow : kFaultRed;
}

std::string tenths(int c10)
{
    char v[16];
    const int a = std::abs(c10);
    std::snprintf(v, sizeof v, "%s%d.%d", c10 < 0 ? "-" : "", a / 10, a % 10);
    return v;
}

}  // namespace

void drawExternalOokStrip(lgfx::LGFX_Sprite &d, int top, const model::OokModel &m, size_t cursor, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    const auto &devs = m.devices();
    const auto &h = m.health();
    const bool live = m.active();
    const bool fault = h.valid && h.hw_fault;
    const bool loss = h.valid && (h.overflows || h.dropped || h.errors || h.ocp_drop);
    Canvas c(d, top);

    obsFrame(c, ModeSigil::PulseBars, "433 MHZ // OOK FIELD",
             fault ? "FAULT" : loss ? "LOSS" : live ? "LIVE" : "CACHED",
             fault ? BadgeTone::Fault : loss ? BadgeTone::Warn : live ? BadgeTone::Live : BadgeTone::Cached);
    obsDock(c, fault ? kFaultRed : live ? kFieldGreen : kMutedSlate, true);

    obsMicro(c, 16, 32, "TRANSMITTER", kDimGreen, kVoidInk);
    obsMicro(c, kTimeX0, 32, "HEARD / LAST 10 MIN", kDimGreen, kVoidInk);
    obsMicro(c, 304, 32, "DBM", kDimGreen, kVoidInk, Align::Right);

    /* A frozen session keeps its shape: time runs to the last packet, not the clock. */
    const uint32_t anchor = !live && !m.packets().empty() ? m.packets().front().received_ms : now;
    const int axis_y = kLaneTop + kLanes * kLaneStep - 4;
    c.line(kTimeX0, axis_y, kTimeX1, axis_y, kDimGreen);
    for (int i = 0; i <= 10; i += 5) {
        const int x = kTimeX0 + (kTimeX1 - kTimeX0) * i / 10;
        c.line(x, axis_y, x, axis_y + 3, kDimGreen);
    }
    obsMicro(c, kTimeX0, axis_y + 5, "-10M", kMutedSlate, kVoidInk);
    obsMicro(c, (kTimeX0 + kTimeX1) / 2, axis_y + 5, "-5M", kMutedSlate, kVoidInk, Align::Center);
    obsMicro(c, kTimeX1, axis_y + 5, live ? "NOW" : "LAST", kMutedSlate, kVoidInk, Align::Right);

    if (devs.empty()) {
        obsMicro(c, 160, 82, live ? "LISTENING FOR 433 MHZ REMOTES / SENSORS" : "NO TRANSMITTERS HEARD",
                 kCalibrationYellow, kVoidInk, Align::Center);
        char f[24];
        if (m.freqKhz()) std::snprintf(f, sizeof f, "%lu.%02lu MHZ OOK", (unsigned long)(m.freqKhz() / 1000),
                                       (unsigned long)(m.freqKhz() % 1000 / 10));
        else std::snprintf(f, sizeof f, "433 MHZ OOK");
        obsText(c, 12, 164, f, FontId::Title, kPaperPhosphor, 220);
        obsMicro(c, 308, 168, live ? "RX ONLY" : "IDLE", kMutedSlate, kPanelDark, Align::Right);
        char v[48];
        std::snprintf(v, sizeof v, "PRINCETON / LACROSSE TX141THBV2  UNK %lu", (unsigned long)m.unknownTrains());
        obsMicro(c, 12, 185, v, kMutedSlate, kPanelDark);
        obsBay(c, 12, 94, "DEVICES", "0", kCalibrationYellow);
        obsBay(c, 122, 86, "PACKETS", std::to_string(m.totalPackets()), kPaperPhosphor);
        obsBay(c, 224, 84, "UNKNOWN", std::to_string(m.unknownTrains()), kMutedSlate);
        return;
    }

    const size_t sel = cursor < devs.size() ? cursor : 0;
    const size_t first = firstVisible(devs.size(), sel, kLanes);
    for (size_t row = 0; row < size_t(kLanes) && first + row < devs.size(); ++row) {
        const size_t i = first + row;
        const auto &dev = devs[i];
        const bool is_sel = i == sel;
        const int y = kLaneTop + int(row) * kLaneStep;
        if (is_sel) {
            c.rect(12, y - 3, 296, kLaneStep - 2, kSelectionGlow);
            c.corners(12, y - 3, 296, kLaneStep - 2, kSignalPink);
        }
        const uint16_t bg = is_sel ? kSelectionGlow : kVoidInk;
        obsMicro(c, 16, y, identity(dev.last), is_sel ? kCalibrationYellow : kPaperPhosphor, bg);
        obsDashedH(c, kTimeX0, kTimeX1, y + 3, kTrackDark, 1, 3);
        for (const auto &p : m.packets()) {
            if (!model::ookSameTransmitter(p, dev.last) || anchor < p.received_ms) continue;
            const uint32_t a = anchor - p.received_ms;
            if (a >= kWindowMs) continue;
            const int x = kTimeX1 - int(uint64_t(a) * (kTimeX1 - kTimeX0) / kWindowMs);
            const uint16_t col = is_sel ? kCalibrationYellow : live ? kFieldGreen : kMutedSlate;
            c.rect(x - 1, y - 1, 3, 9, col);
        }
        const auto &p = dev.last;
        obsMicro(c, 304, y, p.has_rssi ? std::to_string(p.rssi) : std::string("--"),
                 p.has_rssi ? rssiTone(p.rssi) : kMutedSlate, bg, Align::Right);
    }
    if (devs.size() > size_t(kLanes)) {
        char more[40];
        std::snprintf(more, sizeof more, "%u-%u OF %u", unsigned(first + 1),
                      unsigned(std::min(devs.size(), first + kLanes)), unsigned(devs.size()));
        obsMicro(c, 16, axis_y + 5, more, kMutedSlate, kVoidInk);
    }

    const auto &dev = devs[sel];
    const auto &p = dev.last;
    const bool lax = p.decoder == model::OokDecoder::LaCrosse;
    char v[64];
    if (lax) std::snprintf(v, sizeof v, "LACROSSE %u / CH %u", p.id, p.ch);
    else std::snprintf(v, sizeof v, "REMOTE %s", obsUpper(p.hex).c_str());
    obsText(c, 12, 164, v, FontId::Title, kPaperPhosphor, 210);
    std::snprintf(v, sizeof v, "%u DEVICES", unsigned(devs.size()));
    obsMicro(c, 308, 168, v, kMutedSlate, kPanelDark, Align::Right);
    if (lax) {
        std::snprintf(v, sizeof v, "TX141THBV2  %s%s  X%lu  FIRST %s", p.batt_low ? "BATT LOW" : "BATT OK",
                      p.test ? "  TEST" : "", (unsigned long)dev.count, age(anchor, dev.first_ms).c_str());
        obsMicro(c, 12, 185, v, p.batt_low ? kSignalPink : kMutedSlate, kPanelDark);
    } else {
        std::snprintf(v, sizeof v, "PT2262 / EV1527  TE %uUS  %u BIT", p.te_us, p.nbits);
        obsMicro(c, 12, 185, v, kMutedSlate, kPanelDark);
    }
    obsMicro(c, 308, 185, "RX ONLY", kMutedSlate, kPanelDark, Align::Right);

    if (lax) {
        obsBay(c, 12, 94, "TEMPERATURE", tenths(p.temp_c10), kCalibrationYellow, "C");
        obsBay(c, 122, 86, "HUMIDITY", std::to_string(p.hum), kPaperPhosphor, "%");
    } else {
        obsBay(c, 12, 94, "HEARD", std::to_string(dev.count), kCalibrationYellow, "X");
        obsBay(c, 122, 86, "RSSI / DBM", p.has_rssi ? std::to_string(p.rssi) : std::string("--"), kPaperPhosphor);
    }
    obsBay(c, 224, 84, "LAST HEARD", age(anchor, p.received_ms), live ? kFieldGreen : kMutedSlate);
}

}  // namespace ui
