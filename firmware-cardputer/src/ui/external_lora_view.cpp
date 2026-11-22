/* LoRa RF pulse cadence and raw frozen payload view; Observatory v2. */
#include "ui/external_lora_view.h"
#include "ui/observatory_chrome.h"
#include <cstdio>

namespace ui {
void drawExternalLoraStrip(lgfx::LGFX_Sprite &d, int top, const model::LoraModel &m,
                           size_t cursor, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    const auto &packets = m.packets();
    const auto *p = cursor < packets.size() ? &packets[cursor] : nullptr;
    const bool live = m.active();
    Canvas c(d, top);

    obsFrame(c, ModeSigil::PulseBars, "LORA PULSE CADENCE",
             live ? "LIVE" : "CACHED",
             live ? BadgeTone::Live : BadgeTone::Cached);
    obsDock(c, live ? kCalibrationYellow : kMutedSlate, true);

    constexpr int x0 = 38, w = 262, top_y = 38, base_y = 102, h = base_y - top_y;
    auto rssiToY = [](int rssi) {
        const int clamped = std::max(-120, std::min(-30, rssi));
        const int lift = std::max(5, std::min(h - 4, (clamped + 120) * h / 90));
        return base_y - lift;
    };

    /* Calibrated dBm reference lines. */
    obsDashedH(c, x0, x0 + w, rssiToY(-40), kTrackDark, 2, 3);
    obsDashedH(c, x0, x0 + w, rssiToY(-80), kTrackDark, 2, 3);
    c.line(x0, base_y, x0 + w, base_y, kDimGreen);
    obsMicro(c, 34, rssiToY(-40) - 3, "-40", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 34, rssiToY(-80) - 3, "-80", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 34, base_y - 4, "-120", kMutedSlate, kVoidInk, Align::Right);

    uint32_t anchor = now;
    if (!live && !packets.empty()) anchor = packets.front().received_ms;

    int sel_x = -1, sel_y = -1;
    for (const auto &pkt : packets) {
        const uint32_t age = anchor - pkt.received_ms;
        if (age >= 30000) continue;
        const int x = x0 + int(uint64_t(30000 - age) * w / 30000);
        const int y = rssiToY(pkt.rssi);
        const int cap = std::max(4, std::min(10, int(pkt.len) / 4));
        if (&pkt == p) {
            sel_x = x; sel_y = y;
            c.rect(x - 6, top_y + 2, 13, base_y - top_y - 2, kSelectionGlow);
            c.line(x, y, x, base_y, kCalibrationYellow);
            c.line(x + 1, y, x + 1, base_y, kCalibrationYellow);
            c.rect(x - cap / 2, y - 1, cap, 3, kCalibrationYellow);
            obsDiamondFill(c, x, y - 5, 3, kSignalPink);
        } else {
            const uint16_t col = live ? kFieldGreen : kMutedSlate;
            c.line(x, y, x, base_y, col);
            c.rect(x - cap / 2, y - 1, cap, 2, col);
        }
    }

    if (p && sel_x >= 0) {
        char tag[24];
        snprintf(tag, sizeof tag, "%ddBm %uB", p->rssi, unsigned(p->len));
        const int bw = int(std::strlen(tag)) * 6 + 10;
        const bool right = sel_x + 14 + bw <= 304;
        const int bx = right ? sel_x + 14 : std::max(x0 + 2, sel_x - 14 - bw);
        c.line(right ? sel_x + 4 : sel_x - 4, sel_y - 5, right ? bx : bx + bw, 42, kSignalPink);
        obsBadge(c, bx, 34, bw, 15, tag, kPanelDark, kSignalPink, kCalibrationYellow);
    } else if (!p) {
        obsMicro(c, 169, 66, "NO RECEIVED FRAME YET", kCalibrationYellow, kVoidInk, Align::Center);
    }

    obsMicro(c, x0, 108, "-30s", kMutedSlate, kVoidInk);
    obsMicro(c, x0 + w / 2, 108, "HEIGHT=RSSI / CAP=LEN", kMutedSlate, kVoidInk, Align::Center);
    obsMicro(c, x0 + w, 108, live ? "NOW" : "HOLD",
             live ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Right);

    /* Dual calibrated meters: RSSI level + zero-centered SNR margin (-20..+15 dB). */
    obsMicro(c, 20, 120, "RSSI LEVEL", kMutedSlate, kVoidInk);
    const int rssi_pct = p ? std::max(0, std::min(100, (p->rssi + 120) * 100 / 90)) : 0;
    obsSegBar(c, 20, 130, 148, 10, 16, rssi_pct, live ? kFieldGreen : kMutedSlate);

    constexpr int sx = 186, sy = 130, sw = 114, sh = 10;
    const int zero_x = sx + sw * 20 / 35;
    obsMicro(c, sx, 120, "SNR MARGIN", kMutedSlate, kVoidInk);
    if (p) {
        char sbuf[16];
        snprintf(sbuf, sizeof sbuf, "%+.1fdB", double(p->snr));
        obsMicro(c, sx + sw, 120, sbuf, p->snr >= 0.0f ? kFieldGreen : kSignalPink, kVoidInk, Align::Right);
    }
    c.rect(sx, sy, sw, sh, kTrackDark);
    if (p) {
        const float clamped = std::max(-20.0f, std::min(15.0f, p->snr));
        const int vx = sx + int((clamped + 20.0f) * sw / 35.0f);
        const int lx = std::min(zero_x, vx);
        const int lw = std::max(1, std::abs(vx - zero_x));
        c.rect(lx, sy, lw, sh, live ? (p->snr >= 0.0f ? kCalibrationYellow : kSignalPink) : kMutedSlate);
    }
    c.line(zero_x, sy - 2, zero_x, sy + sh + 2, kPaperPhosphor);

    /* Lower Reading Dock. */
    char v[48];
    if (m.hasConfig()) {
        snprintf(v, sizeof v, "%lu.%03lu MHz",
                 (unsigned long)(m.freqHz() / 1000000),
                 (unsigned long)((m.freqHz() % 1000000) / 1000));
        obsText(c, 12, 164, v, FontId::Title, kPaperPhosphor, 114);
        snprintf(v, sizeof v, "SF%d BW%d", m.sf(), m.bwKhz());
        obsBadge(c, 126, 164, 72, 15, v, kTrackDark, kDimGreen, kFieldGreen);
    } else {
        obsText(c, 12, 164, "LORA / NO CONFIG", FontId::Title, kCalibrationYellow, 194);
    }
    std::string guess = !p ? "WAIT"
                      : p->framing == model::Framing::Unknown ? "UNCLASSIFIED?"
                      : std::string(model::framingName(p->framing)) + "?";
    for (char &ch : guess) if (ch >= 'a' && ch <= 'z') ch = char(ch - 32);
    obsMicro(c, 308, 168, guess, kMutedSlate, kPanelDark, Align::Right);
    obsMicro(c, 12, 185, "RAW RX FRAME / NO PAYLOAD DECODE", kMutedSlate, kPanelDark);
    if (m.health().valid) {
        snprintf(v, sizeof v, "RX %lu CRC %lu",
                 (unsigned long)m.health().rx, (unsigned long)m.health().crc_err);
        obsMicro(c, 308, 185, v, m.health().crc_err ? kSignalPink : kMutedSlate, kPanelDark, Align::Right);
    }

    obsBay(c, 12, 94, "RSSI / dBm", p ? std::to_string(p->rssi) : "--", kCalibrationYellow);
    if (p) snprintf(v, sizeof v, "%+.1f", double(p->snr));
    else snprintf(v, sizeof v, "--");
    obsBay(c, 122, 86, "SNR / dB", v, kFieldGreen);
    obsBay(c, 224, 84, "BYTES", p ? std::to_string(p->len) : "--", kPaperPhosphor, p ? "RAW" : "");
}

void drawExternalLoraDetailStrip(lgfx::LGFX_Sprite &d, int top, const model::PacketInspection &p)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const auto &packet = p.packet();
    obsFrame(c, ModeSigil::PulseBars, "CAPTURED PAYLOAD // RAW", "FROZEN", BadgeTone::Warn);
    obsDock(c, kCalibrationYellow, false);

    std::string framing = packet.framing == model::Framing::Unknown
                        ? "UNCLASSIFIED"
                        : std::string(model::framingName(packet.framing)) + "?";
    for (char &ch : framing) if (ch >= 'a' && ch <= 'z') ch = char(ch - 32);
    obsText(c, 16, 32, framing, FontId::Title, kCalibrationYellow, 150);
    obsMicro(c, 304, 36, "FRAMING GUESS / UNDECODED", kMutedSlate, kVoidInk, Align::Right);
    c.line(16, 51, 304, 51, kLineBorder);

    for (size_t row = 0; row < 8; ++row) {
        auto line = p.line(row);
        if (!line.bytes) break;
        char offset[8];
        snprintf(offset, sizeof offset, "%02X", unsigned(p.offset() + row * 8));
        const int y = 57 + int(row) * 11;
        obsMicro(c, 16, y, offset, kMutedSlate, kVoidInk);
        obsMicro(c, 42, y, line.hex, kPaperPhosphor, kVoidInk);
        obsMicro(c, 214, y, line.ascii, kFieldGreen, kVoidInk);
    }

    char meta[48];
    snprintf(meta, sizeof meta, "%u BYTES / %d dBm", unsigned(packet.len), packet.rssi);
    obsText(c, 12, 165, meta, FontId::Title, kCalibrationYellow, 200);
    snprintf(meta, sizeof meta, "SNR %+.1f dB", double(packet.snr));
    obsText(c, 308, 168, meta, FontId::Mono, kPaperPhosphor, 100, Align::Right);
    obsMicro(c, 12, 186, "FROZEN RAW PAYLOAD SNAPSHOT", kMutedSlate, kPanelDark);

    const size_t page = p.offset() / 64 + 1;
    const size_t pages = std::max(size_t(1), (packet.hex.size() / 2 + 63) / 64);
    obsBay(c, 12, 140, "PAGE", std::to_string(page) + "/" + std::to_string(pages), kPaperPhosphor);
    obsBay(c, 168, 140, "PAYLOAD", std::to_string(packet.len), kFieldGreen, "BYTES");
}

void drawExternalLoraSweepStrip(lgfx::LGFX_Sprite &d, int top,
                                const model::LoraSweepModel &sw, uint32_t now)
{
    (void)now;
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const bool live = sw.active();
    const bool has = sw.hasData();

    obsFrame(c, ModeSigil::PulseBars, "SUB-GHZ SPECTRUM",
             live ? "LIVE" : has ? "CACHED" : "IDLE",
             live ? BadgeTone::Live : BadgeTone::Cached);
    obsDock(c, live ? kCalibrationYellow : kMutedSlate, true);

    constexpr int x0 = 38, w = 262, top_y = 34, base_y = 94, h = base_y - top_y;
    auto dbmToY = [](int dbm) {
        const int clamped = std::max(-120, std::min(-40, dbm));
        const int lift = std::max(1, std::min(h - 2, (clamped + 120) * (h - 2) / 80));
        return base_y - lift;
    };
    auto wfColor = [](int dbm, int floor_dbm) -> uint16_t {
        const int above = dbm - floor_dbm;
        if (above >= 28 || dbm >= -65) return kSignalPink;
        if (above >= 16 || dbm >= -80) return kCalibrationYellow;
        if (above >= 8 || dbm >= -96) return kFieldGreen;
        if (above >= 3) return kDimGreen;
        return kTrackDark;
    };

    obsDashedH(c, x0, x0 + w, dbmToY(-50), kTrackDark, 2, 3);
    obsDashedH(c, x0, x0 + w, dbmToY(-85), kTrackDark, 2, 3);
    c.line(x0, base_y, x0 + w, base_y, kDimGreen);
    obsMicro(c, 34, dbmToY(-50) - 3, "-50", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 34, dbmToY(-85) - 3, "-85", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 34, base_y - 4, "-120", kMutedSlate, kVoidInk, Align::Right);

    if (has && sw.bins() > 0) {
        const size_t n = sw.bins();
        for (size_t i = 0; i < n; ++i) {
            const int bx0 = x0 + static_cast<int>(i * w / n);
            const int bx1 = x0 + static_cast<int>((i + 1) * w / n);
            const int bw = std::max(1, bx1 - bx0 - 1);
            const int cy = dbmToY(sw.currentDbm(i));
            const int py = dbmToY(sw.peakDbm(i));
            c.rect(bx0, cy, bw, base_y - cy, live ? kFieldGreen : kMutedSlate);
            c.rect(bx0, py - 1, bw, 2, i == sw.peakBin() ? kSignalPink : kCalibrationYellow);
        }

        /* Peak callout badge. */
        const uint32_t pk_khz = sw.binFreqKhz(sw.peakBin());
        char tag[28];
        snprintf(tag, sizeof tag, "%lu.%02luM %ddBm",
                 (unsigned long)(pk_khz / 1000), (unsigned long)((pk_khz % 1000) / 10),
                 static_cast<int>(sw.maxPeakDbm()));
        const int tw = int(std::strlen(tag)) * 6 + 10;
        obsBadge(c, x0 + w - tw, 32, tw, 15, tag, kPanelDark, kSignalPink, kCalibrationYellow);

        /* 24-row scrolling waterfall strip (y=101..149). */
        constexpr int wy = 101;
        c.rect(x0, wy, w, 48, kVoidInk);
        const size_t rows = std::min<size_t>(sw.historyCount(), 24);
        for (size_t r = 0; r < rows; ++r) {
            const int ry = wy + static_cast<int>(r) * 2;
            for (size_t i = 0; i < n; ++i) {
                const int bx0 = x0 + static_cast<int>(i * w / n);
                const int bx1 = x0 + static_cast<int>((i + 1) * w / n);
                const uint16_t col = wfColor(sw.historyDbm(r, i), sw.floorDbm());
                if (col != kTrackDark) c.rect(bx0, ry, std::max(1, bx1 - bx0), 2, col);
            }
        }
        obsMicro(c, 34, wy + 2, "NOW", kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, 34, wy + 40, "OLD", kMutedSlate, kVoidInk, Align::Right);
    } else {
        obsMicro(c, x0 + w / 2, 62, live ? "SWEEPING SX1262 GETRSSIINST..." : "SUB-GHZ SWEEP IDLE",
                 kCalibrationYellow, kVoidInk, Align::Center);
        obsMicro(c, x0 + w / 2, 120, "WATERFALL WAITING FOR SWEEP", kMutedSlate, kVoidInk, Align::Center);
    }

    char v[48];
    const uint32_t s_khz = sw.startKhz();
    const uint32_t e_khz = sw.bins() ? sw.endKhz() : (s_khz + 63u * uint32_t(sw.stepKhz()));
    snprintf(v, sizeof v, "%lu.%lu-%lu.%lu MHz",
             (unsigned long)(s_khz / 1000), (unsigned long)((s_khz % 1000) / 100),
             (unsigned long)(e_khz / 1000), (unsigned long)((e_khz % 1000) / 100));
    obsText(c, 12, 164, v, FontId::Title, kPaperPhosphor, 178);
    snprintf(v, sizeof v, "%u BINS", static_cast<unsigned>(sw.bins() ? sw.bins() : 64));
    obsBadge(c, 194, 164, 54, 15, v, kTrackDark, kDimGreen, kFieldGreen);
    obsMicro(c, 308, 168, "RX ONLY", kMutedSlate, kPanelDark, Align::Right);
    obsMicro(c, 12, 185, "SX1262 PASSIVE ENERGY // PEAK HOLD + WATERFALL", kMutedSlate, kPanelDark);

    if (has) {
        const uint32_t pk_khz = sw.binFreqKhz(sw.peakBin());
        snprintf(v, sizeof v, "%lu.%02lu",
                 (unsigned long)(pk_khz / 1000), (unsigned long)((pk_khz % 1000) / 10));
        obsBay(c, 12, 94, "PEAK / MHz", v, kCalibrationYellow);
        obsBay(c, 122, 86, "PEAK / dBm", std::to_string(sw.maxPeakDbm()), kSignalPink);
        obsBay(c, 224, 84, "FLOOR", std::to_string(sw.floorDbm()), kFieldGreen,
               ("S" + std::to_string(sw.sweeps())).c_str());
    } else {
        obsBay(c, 12, 94, "PEAK / MHz", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "PEAK / dBm", "--", kSignalPink);
        obsBay(c, 224, 84, "FLOOR", "--", kFieldGreen);
    }
}
}  // namespace ui
