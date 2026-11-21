/* One measured scope or channel dwell field; Observatory v2. */
#include "ui/external_spectrum_view.h"
#include "ui/external_plot.h"
#include "ui/observatory_chrome.h"
#include <cstdio>

namespace ui {
void drawExternalSpectrumStrip(lgfx::LGFX_Sprite &d, int top,
                               const model::SpectrumModel &s, size_t cursor, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    const auto &rows = s.readings();
    if (s.locked()) cursor = 0;
    const auto *r = cursor < rows.size() ? &rows[cursor] : nullptr;
    const bool fresh = r && r->valid && now - r->received_ms < 3000;
    const char *state = !s.active() ? "CACHED" : !r || !r->valid ? "WAIT" : fresh ? "LIVE" : "AGED";
    const BadgeTone tone = !s.active() ? BadgeTone::Cached
                         : (r && r->valid && fresh) ? BadgeTone::Live : BadgeTone::Warn;
    Canvas c(d, top);

    obsFrame(c, ModeSigil::WaveScope,
             s.locked() ? "PACKET SCOPE // 1S" : "CHANNEL FIELD // DWELL",
             state, tone);
    obsDock(c, tone == BadgeTone::Live ? kFieldGreen : tone == BadgeTone::Warn ? kCalibrationYellow : kMutedSlate, true);

    constexpr int x0 = 34, y0 = 46, w = 264, h = 84;
    for (int i = 0; i <= 4; ++i) {
        const int yy = y0 + (h * i) / 4;
        if (i == 4) c.line(x0, yy, x0 + w, yy, kDimGreen);
        else obsDashedH(c, x0, x0 + w, yy, kTrackDark, 2, 3);
    }
    for (int i = 0; i <= 6; ++i) {
        obsDashedV(c, x0 + (w * i) / 6, y0, y0 + h, kTrackDark, 2, 3);
    }

    uint32_t peak = 0;
    uint64_t sum = 0;
    size_t valid_count = 0;
    bool have_peak = false;

    if (s.locked() && r) {
        const size_t count = std::min(r->history_size, r->history.size());
        const size_t first = (r->history_next + r->history.size() - count) % r->history.size();
        uint32_t anchor = now;
        if (!s.active() && count) anchor = r->history[(first + count - 1) % r->history.size()].received_ms;
        for (size_t i = 0; i < count; ++i) {
            const auto &sample = r->history[(first + i) % r->history.size()];
            if (anchor - sample.received_ms < 30000) {
                peak = std::max(peak, sample.pkts);
                sum += sample.pkts;
                ++valid_count;
            }
        }
        have_peak = valid_count != 0;
        const uint32_t scale = plot::scaleFor(peak);
        char lbl[16];
        snprintf(lbl, sizeof lbl, "%lu", (unsigned long)scale);
        obsMicro(c, 30, y0 - 3, lbl, kMutedSlate, kVoidInk, Align::Right);
        snprintf(lbl, sizeof lbl, "%lu", (unsigned long)(scale / 2));
        obsMicro(c, 30, y0 + h / 2 - 3, lbl, kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, 30, y0 + h - 4, "0", kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, x0, 32, "PKTS / 1S WINDOW", kDimGreen, kVoidInk);

        int peak_x = x0 + w / 2;
        int peak_y = y0 + h;
        int prev_x = 0, prev_y = 0;
        uint32_t prev_ms = 0;
        bool has_prev = false;
        const uint16_t trace_col = s.active() ? kFieldGreen : kMutedSlate;
        bool peak_marked = false;

        if (have_peak) {
            peak_y = y0 + h - int(uint64_t(peak) * h / scale);
            obsDashedH(c, x0, x0 + w, peak_y, kSignalPink, 3, 3);
        }

        for (size_t i = 0; i < count; ++i) {
            const auto &sample = r->history[(first + i) % r->history.size()];
            const uint32_t age = anchor - sample.received_ms;
            if (age >= 30000) continue;
            const int px = x0 + int(uint64_t(30000 - age) * w / 30000);
            const int py = y0 + h - int(uint64_t(sample.pkts) * h / scale);
            const bool is_peak = !peak_marked && sample.pkts == peak;
            if (is_peak) { peak_x = px; peak_marked = true; }
            const bool connected = has_prev && (sample.received_ms - prev_ms <= 1600);
            /* Solid vertical phosphor drop-stem per measured window (missing windows stay gaps). */
            c.line(px, py + 2, px, y0 + h - 1, is_peak ? kLineBorder : kTrackDark);
            if (connected) {
                c.line(prev_x, prev_y + 1, px, py + 1, kDimGreen);
                c.line(prev_x, prev_y, px, py, trace_col);
            }
            if (is_peak) obsDiamondFill(c, px, py, 3, kSignalPink);
            else c.rect(px - 1, py - 1, 3, 3, trace_col);
            prev_x = px; prev_y = py; prev_ms = sample.received_ms; has_prev = true;
        }
        if (has_prev) {
            c.ring(prev_x, prev_y, 5, kSignalPink);
            c.rect(prev_x - 2, prev_y - 2, 5, 5, kCalibrationYellow);
        }
        if (have_peak) {
            char pbuf[20];
            snprintf(pbuf, sizeof pbuf, "PEAK %lu", (unsigned long)peak);
            const int bx = std::max(x0 + 104, std::min(x0 + w - 58, peak_x - 28));
            obsBadge(c, bx, 29, 56, 13, pbuf, kPanelDark, kSignalPink, kSignalPink);
        } else {
            obsMicro(c, 160, 84, r->valid ? "NO RECENT MEASURED WINDOWS" : "WAITING FOR MEASURED WINDOWS",
                     kCalibrationYellow, kVoidInk, Align::Center);
        }
        obsMicro(c, x0, 136, "-30s", kMutedSlate, kVoidInk);
        obsMicro(c, x0 + w / 2, 136, "-15s / MEASURED ONLY", kMutedSlate, kVoidInk, Align::Center);
        obsMicro(c, x0 + w, 136, s.active() ? "NOW" : "FROZEN",
                 s.active() ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Right);
    } else {
        const size_t first = firstVisible(rows.size(), cursor);
        const size_t count = std::min(size_t(12), rows.size() - first);
        uint32_t max_pkts = 0;
        for (size_t i = 0; i < count; ++i) {
            if (rows[first + i].valid) {
                max_pkts = std::max(max_pkts, rows[first + i].pkts);
                sum += rows[first + i].pkts;
                ++valid_count;
            }
        }
        have_peak = valid_count != 0;
        const uint32_t scale = plot::scaleFor(max_pkts);
        char lbl[16];
        snprintf(lbl, sizeof lbl, "%lu", (unsigned long)scale);
        obsMicro(c, 30, y0 - 3, lbl, kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, 30, y0 + h - 4, "0", kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, x0, 32, "DWELL PACKETS / CHANNEL", kDimGreen, kVoidInk);

        const int step = count ? w / int(count) : w;
        for (size_t i = 0; i < count; ++i) {
            const auto &item = rows[first + i];
            const bool sel = first + i == cursor;
            const int x = x0 + int(i) * step;
            const int bw = std::max(6, step - 5);
            const int bh = std::max(2, int(uint64_t(item.pkts) * h / scale));
            const bool aged = !s.active() || now - item.received_ms >= 3000;
            if (item.valid) {
                c.rect(x, y0 + h - bh, bw, bh, sel ? kDimGreen : kTrackDark);
                c.line(x, y0 + h - bh, x + bw - 1, y0 + h - bh,
                       sel ? kCalibrationYellow : aged ? kMutedSlate : kFieldGreen);
                for (int yy = y0 + h - bh + 5; yy < y0 + h; yy += 6) {
                    c.line(x, yy, x + bw - 1, yy, aged ? kLineBorder : kDimGreen);
                }
            }
            if (sel) c.corners(x - 2, y0 - 2, bw + 4, h + 4, kSignalPink);
            snprintf(lbl, sizeof lbl, "%u", unsigned(item.ch));
            obsMicro(c, x + bw / 2, 136, lbl, sel ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Center);
        }
        if (!count) {
            obsMicro(c, 160, 84, "WAITING FOR CHANNEL OBSERVATIONS", kCalibrationYellow, kVoidInk, Align::Center);
        }
        peak = max_pkts;
    }

    char head[40];
    if (r) snprintf(head, sizeof head, "CH %02u / %s", unsigned(r->ch), r->band5 ? "5 GHz" : "2.4 GHz");
    else snprintf(head, sizeof head, "NO CHANNEL SELECTED");
    obsText(c, 12, 164, head, FontId::Title, kPaperPhosphor, 296);

    char sub[48];
    if (s.locked()) snprintf(sub, sizeof sub, "LOCKED CHANNEL / %u WINDOWS", unsigned(valid_count));
    else snprintf(sub, sizeof sub, "CHANNEL SURVEY / %u VISIBLE", unsigned(valid_count));
    obsMicro(c, 12, 185, sub, kMutedSlate, kPanelDark);
    if (r && r->valid) {
        snprintf(sub, sizeof sub, "%lus OLD", (unsigned long)((now - r->received_ms) / 1000));
        obsMicro(c, 308, 185, sub, fresh ? kMutedSlate : kCalibrationYellow, kPanelDark, Align::Right);
    }

    const std::string latest = (r && r->valid) ? std::to_string(r->pkts) : "--";
    char mean_str[16] = "--";
    if (valid_count) snprintf(mean_str, sizeof mean_str, "%.1f", double(sum) / double(valid_count));

    obsBay(c, 12, 94, s.locked() ? "LATEST 1S" : "LATEST DWELL", latest, kFieldGreen, "pkts");
    obsBay(c, 122, 86, s.locked() ? "PEAK / 30S" : "VISIBLE MAX",
           have_peak ? std::to_string(peak) : "--", kSignalPink, "pkts");
    obsBay(c, 224, 84, s.locked() ? "30S MEAN" : "MEAN", mean_str, kPaperPhosphor, s.locked() ? "/s" : "");
}
}  // namespace ui
