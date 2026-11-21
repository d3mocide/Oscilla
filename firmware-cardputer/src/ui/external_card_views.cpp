/* Focused graphical deauth, correlation and settings instruments; Observatory v2. */
#include "ui/external_card_views.h"
#include "ui/observatory_chrome.h"
#include <array>
#include <cstdio>

namespace ui {
void drawExternalDeauthStrip(lgfx::LGFX_Sprite &d, int top, const model::DeauthModel &m,
                             size_t cursor, bool recent, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const bool live = m.active();
    obsFrame(c, ModeSigil::ShieldAlert, "MANAGEMENT EVENTS // RX",
             live ? "LIVE" : "CACHED",
             live ? BadgeTone::Live : BadgeTone::Cached);
    obsDock(c, recent && live ? kSignalPink : live ? kFieldGreen : kMutedSlate, true);

    const auto *selected = cursor < m.events().size() ? &m.events()[cursor] : nullptr;
    std::array<unsigned, 64> channels{}, counts{};
    size_t n = 0, focus = 0;
    unsigned max_cnt = 1;
    for (const auto &r : m.events()) {
        size_t i = 0;
        while (i < n && channels[i] != r.ch) ++i;
        if (i == n && n < channels.size()) channels[n++] = r.ch;
        if (i < counts.size()) max_cnt = std::max(max_cnt, ++counts[i]);
        if (selected && r.ch == selected->ch) focus = i;
    }

    constexpr int x0 = 36, y0 = 46, w = 264, h = 78;
    for (int i = 0; i <= 4; ++i) {
        const int yy = y0 + (h * i) / 4;
        if (i == 4) c.line(x0, yy, x0 + w, yy, kDimGreen);
        else obsDashedH(c, x0, x0 + w, yy, kTrackDark, 2, 3);
    }
    obsMicro(c, 32, y0 - 3, std::to_string(max_cnt), kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 32, y0 + h - 4, "0", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, x0, 32, "EVENTS BY CHANNEL", kDimGreen, kVoidInk);
    if (recent && live) {
        const uint16_t pulse = (now / 150) % 2 ? kSignalPink : kCalibrationYellow;
        obsBadge(c, 260, 29, 40, 13, "BURST", kPanelDark, pulse, pulse);
    }

    const size_t first = firstVisible(n, focus);
    const size_t visible = std::min(size_t(12), n - first);
    const int step = visible ? std::min(36, w / int(visible)) : w;
    const int start_x = x0 + (w - step * int(visible)) / 2;
    const int bw = std::max(8, step - 6);
    for (size_t i = 0; i < visible; ++i) {
        const int bh = std::max(2, int(counts[first + i] * h / max_cnt));
        const int x = start_x + int(i) * step + 3;
        const bool focused = first + i == focus;
        c.rect(x, y0 + h - bh, bw, bh, focused ? kSignalPink : kDimGreen);
        c.line(x, y0 + h - bh, x + bw - 1, y0 + h - bh, focused ? kCalibrationYellow : kFieldGreen);
        if (focused) c.corners(x - 2, y0 - 2, bw + 4, h + 4, kCalibrationYellow);
        obsMicro(c, x + bw / 2, 130, std::to_string(channels[first + i]),
                 focused ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Center);
    }

    if (!selected) {
        obsMicro(c, 160, 82, live ? "WATCHING / NO EVENTS" : "NO RECORDED EVENTS",
                 kCalibrationYellow, kVoidInk, Align::Center);
        obsText(c, 12, 164, "PASSIVE DETECTION", FontId::Title, kPaperPhosphor, 296);
        obsMicro(c, 12, 185, "DEAUTH / DISASSOC MONITOR", kMutedSlate, kPanelDark);
        obsBay(c, 12, 94, "REASON", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "RSSI / dBm", "--", kPaperPhosphor);
        obsBay(c, 224, 84, "CHANNEL", "--", kMutedSlate);
        return;
    }

    obsText(c, 12, 164, selected->disassoc ? "DISASSOCIATION" : "DEAUTHENTICATION",
            FontId::Title, kSignalPink, 296);
    obsText(c, 12, 184, selected->mac, FontId::Mono, kMutedSlate, 180);
    obsMicro(c, 308, 186, std::to_string(m.events().size()) + " TOTAL", kMutedSlate, kPanelDark, Align::Right);

    obsBay(c, 12, 94, "REASON", std::to_string(selected->reason), kCalibrationYellow);
    obsBay(c, 122, 86, "RSSI / dBm", std::to_string(selected->rssi), kPaperPhosphor);
    obsBay(c, 224, 84, "CHANNEL", std::to_string(selected->ch), kFieldGreen);
}

void drawExternalAntiStrip(lgfx::LGFX_Sprite &d, int top, const model::AntiSurveillanceModel &m,
                           size_t cursor, bool starting, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const char *st = starting ? "START" : m.active() ? "WATCH" : "CACHED";
    const BadgeTone tone = starting ? BadgeTone::Warn : m.active() ? BadgeTone::Live : BadgeTone::Cached;
    obsFrame(c, ModeSigil::ShieldAlert, "MOVEMENT CORRELATION", st, tone);

    const auto *r = cursor < m.trackers().size() ? &m.trackers()[cursor] : nullptr;
    obsDock(c, r && r->alert ? kSignalPink : m.active() ? kFieldGreen : kMutedSlate, true);

    /* Three-stage spatial correlation vector (SEEN -> 25m -> 50m). */
    obsMicro(c, 20, 33, "COINCIDENT SIGHTING STAGES", kDimGreen, kVoidInk);
    constexpr int sy = 80;
    for (int i = 0; i < 3; ++i) {
        const int x = 56 + i * 104;
        const bool reached = r && r->movement_legs >= i;
        if (i) {
            const uint16_t lc = reached ? (i == 2 && r->alert ? kSignalPink : kFieldGreen) : kTrackDark;
            c.line(x - 86, sy, x - 18, sy, lc);
            if (reached) c.line(x - 86, sy + 1, x - 18, sy + 1, kDimGreen);
        }
        if (i == 2 && r && r->alert) obsHexagon(c, x, sy, 22, kSignalPink);
        c.ring(x, sy, 15, reached ? (i == 2 && r->alert ? kSignalPink : kFieldGreen) : kTrackDark);
        c.ring(x, sy, 10, reached ? kDimGreen : kLineBorder);
        if (reached) {
            c.dot(x, sy, 5, i == 2 && r->alert ? kSignalPink : kCalibrationYellow);
        }
        obsMicro(c, x, 104, i ? std::to_string(i * 25) + "m" : "SEEN",
                 reached ? kPaperPhosphor : kMutedSlate, kVoidInk, Align::Center);
    }

    obsBadge(c, 20, 127, 280, 16,
             m.hasPosition() ? "GNSS MOVEMENT CONTEXT ACTIVE" : "MOVEMENT PAUSED / NO FRESH GNSS",
             kPanelDark, m.hasPosition() ? kDimGreen : kCalibrationYellow,
             m.hasPosition() ? kMutedSlate : kCalibrationYellow);

    if (!r) {
        obsText(c, 12, 164, "NO TRACKER OBSERVATIONS", FontId::Title, kPaperPhosphor, 296);
        obsMicro(c, 12, 185, "PASSIVE MOVEMENT CORRELATION", kMutedSlate, kPanelDark);
        obsBay(c, 12, 94, "SIGHTINGS", "0", kCalibrationYellow);
        obsBay(c, 122, 86, "LEGS", "0", kPaperPhosphor);
        obsBay(c, 224, 84, "AGE / s", "--", kMutedSlate);
        return;
    }

    obsText(c, 12, 164, r->mac, FontId::Title, r->alert ? kSignalPink : kPaperPhosphor, 296);
    obsMicro(c, 12, 185,
             r->alert ? "REVIEW PATTERN / NOT PROOF OF FOLLOWING" : "COINCIDENT SIGHTINGS / NO ALERT",
             r->alert ? kSignalPink : kMutedSlate, kPanelDark);

    obsBay(c, 12, 94, "SIGHTINGS", std::to_string(r->sightings), kCalibrationYellow);
    obsBay(c, 122, 86, "LEGS", std::to_string(r->movement_legs), kPaperPhosphor, "/ 2");
    obsBay(c, 224, 84, "AGE / s", std::to_string((now - r->last_seen_ms) / 1000), kMutedSlate, "s");
}

void drawExternalSettingsStrip(lgfx::LGFX_Sprite &d, int top, uint8_t brightness)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const int percent = (brightness * 100U + 127U) / 255U;

    obsFrame(c, ModeSigil::SystemCore, "DECK DISPLAY // LUMINANCE", "SETTING", BadgeTone::Cached);
    obsDock(c, kCalibrationYellow, false);

    obsCard(c, 36, 44, 248, 86, kFieldGreen);
    obsMicro(c, 48, 52, "CARDPUTER LCD BACKLIGHT", kMutedSlate, kPanelDark);
    obsMicro(c, 272, 52, std::to_string(percent) + "%", kCalibrationYellow, kPanelDark, Align::Right);
    obsSegBar(c, 48, 70, 224, 34, 16, percent, kFieldGreen);
    obsMicro(c, 48, 112, "0%", kMutedSlate, kPanelDark);
    obsMicro(c, 160, 112, "50%", kMutedSlate, kPanelDark, Align::Center);
    obsMicro(c, 272, 112, "100%", kMutedSlate, kPanelDark, Align::Right);

    obsText(c, 12, 164, "DECK DISPLAY", FontId::Title, kPaperPhosphor, 296);
    obsMicro(c, 12, 185, "ADJUSTS CARDPUTER PANEL ONLY / EXTERNAL FIXED", kMutedSlate, kPanelDark);

    obsBay(c, 12, 140, "BRIGHTNESS", std::to_string(percent) + "%", kCalibrationYellow);
    obsBay(c, 168, 140, "EXTERNAL TFT", "FIXED ON", kFieldGreen);
}
}  // namespace ui
