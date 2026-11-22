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

void drawExternalSettingsStrip(lgfx::LGFX_Sprite &d, int top, const SettingsViewState &s)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const size_t cur = s.cursor < kSettingsRowCount ? s.cursor : 0;
    const auto row = static_cast<SettingsRow>(cur);
    const int percent = (s.brightness * 100U + 127U) / 255U;

    const std::string header = std::string("CONFIG // ") + settingsRowName(row);
    obsFrame(c, ModeSigil::SystemCore, header.c_str(),
             s.sd_ready ? "SD SAVED" : "RAM ONLY",
             s.sd_ready ? BadgeTone::Live : BadgeTone::Warn);

    uint16_t dock_accent = kCalibrationYellow;
    if (row == SettingsRow::DebugSerial && s.debug_enabled) dock_accent = kSignalPink;
    else if (row == SettingsRow::Brightness || (row == SettingsRow::WifiRegion && s.region_ok) ||
             (row == SettingsRow::WifiBand && s.band_ok)) dock_accent = kFieldGreen;
    obsDock(c, dock_accent, true);

    const char *dock_title = settingsRowName(row);
    const char *dock_desc = "";

    switch (row) {
    case SettingsRow::Brightness:
        obsCard(c, 20, 40, 280, 98, kFieldGreen);
        obsMicro(c, 32, 48, "CARDPUTER LCD BACKLIGHT", kMutedSlate, kPanelDark);
        obsMicro(c, 288, 48, std::to_string(percent) + "%", kCalibrationYellow, kPanelDark, Align::Right);
        obsSegBar(c, 32, 66, 256, 36, 16, percent, kFieldGreen);
        obsMicro(c, 32, 110, "0%", kMutedSlate, kPanelDark);
        obsMicro(c, 160, 110, "50%", kMutedSlate, kPanelDark, Align::Center);
        obsMicro(c, 288, 110, "100%", kMutedSlate, kPanelDark, Align::Right);
        obsMicro(c, 160, 124, "EXTERNAL ST7789 BACKLIGHT FIXED ON 3V3", kDimGreen, kPanelDark, Align::Center);
        dock_title = "DISPLAY BRIGHTNESS";
        dock_desc = "ADJUSTS INTERNAL LCD ONLY / /oscilla/brightness";
        break;

    case SettingsRow::IdleSleep: {
        obsCard(c, 20, 40, 280, 98, s.idle_timeout_s ? kFieldGreen : kCalibrationYellow);
        obsMicro(c, 32, 48, "AUTO DISPLAY SLEEP", kMutedSlate, kPanelDark);
        obsMicro(c, 288, 48, s.idle_timeout_s ? "ENABLED" : "DISABLED",
                 s.idle_timeout_s ? kFieldGreen : kMutedSlate, kPanelDark, Align::Right);
        static constexpr uint16_t kVals[5] = {0, 30, 60, 120, 300};
        static constexpr const char *kLbls[5] = {"OFF", "30s", "60s", "2m", "5m"};
        for (int i = 0; i < 5; ++i) {
            const int px = 30 + i * 53;
            const bool active = s.idle_timeout_s == kVals[i];
            c.rect(px, 68, 48, 30, active ? kSelectionGlow : kVoidInk);
            c.box(px, 68, 48, 30, active ? kCalibrationYellow : kLineBorder);
            obsText(c, px + 24, 76, kLbls[i], FontId::Head,
                    active ? kCalibrationYellow : kMutedSlate, 44, Align::Center);
        }
        obsMicro(c, 160, 110, "LCD OFF + TFT SLPIN / RADIOS & GNSS LIVE",
                 kMutedSlate, kPanelDark, Align::Center);
        obsMicro(c, 160, 124, "ANY KEY WAKES WITHOUT ACTING", kDimGreen, kPanelDark, Align::Center);
        dock_title = "IDLE POWER SAVING";
        dock_desc = "PERSISTED IN /oscilla/idle_timeout";
        break;
    }

    case SettingsRow::WifiRegion:
        obsCard(c, 20, 40, 280, 98, s.region_ok ? kFieldGreen : kCalibrationYellow);
        obsMicro(c, 32, 48, "REGULATORY DOMAIN", kMutedSlate, kPanelDark);
        obsBadge(c, 212, 45, 76, 15, s.region_status, kTrackDark,
                 s.region_ok ? kFieldGreen : kCalibrationYellow,
                 s.region_ok ? kFieldGreen : kCalibrationYellow);
        obsText(c, 32, 62, s.region_code, FontId::Hero, kCalibrationYellow, 96);
        obsMicro(c, 132, 68, "PASSIVE DFS-FREE HOP TABLE", kMutedSlate, kPanelDark);
        obsMicro(c, 132, 82, "PUSHED TO PROBE WHEN IDLE", kDimGreen, kPanelDark);
        c.line(32, 98, 288, 98, kLineBorder);
        obsMicro(c, 32, 105, "CHANNELS", kMutedSlate, kPanelDark);
        obsText(c, 32, 117, s.region_channels, FontId::Mono, kPaperPhosphor, 256);
        dock_title = "WI-FI REGION";
        dock_desc = "COUNTRY & HOP LIST / /oscilla/wifi_region";
        break;

    case SettingsRow::WifiBand: {
        obsCard(c, 20, 40, 280, 98, s.band_ok ? kFieldGreen : kCalibrationYellow);
        obsMicro(c, 32, 48, "ACTIVE WI-FI BAND SCOPE", kMutedSlate, kPanelDark);
        obsMicro(c, 288, 48, s.band_ok ? "PROBE SYNCED" : "PENDING IDLE",
                 s.band_ok ? kFieldGreen : kCalibrationYellow, kPanelDark, Align::Right);
        static constexpr const char *kCodes[3] = {"24", "auto", "5"};
        static constexpr const char *kNames[3] = {"2.4 GHz", "AUTO", "5 GHz"};
        for (int i = 0; i < 3; ++i) {
            const int px = 30 + i * 88;
            const bool active = s.band_code == kCodes[i];
            c.rect(px, 68, 82, 34, active ? kSelectionGlow : kVoidInk);
            c.box(px, 68, 82, 34, active ? kCalibrationYellow : kLineBorder);
            obsText(c, px + 41, 78, kNames[i], FontId::Head,
                    active ? kCalibrationYellow : kMutedSlate, 76, Align::Center);
        }
        obsMicro(c, 160, 113, "FILTERS SCANS, SNIFFER, DEAUTH & SPECTRUM",
                 kMutedSlate, kPanelDark, Align::Center);
        obsMicro(c, 160, 125, "CHANNEL REGION: " + s.region_code, kDimGreen, kPanelDark, Align::Center);
        dock_title = "WI-FI BAND FILTER";
        dock_desc = "2.4G, 5G, OR DUAL / /oscilla/wifi_band";
        break;
    }

    case SettingsRow::LoraProfile: {
        obsCard(c, 20, 40, 280, 98, kCalibrationYellow);
        obsMicro(c, 32, 48, "SUB-GHZ DEFAULT PRESET", kMutedSlate, kPanelDark);
        obsBadge(c, 214, 45, 74, 15, s.lora_from_sd ? "SD CARD" : "BUILT-IN",
                 kTrackDark, s.lora_from_sd ? kCalibrationYellow : kDimGreen,
                 s.lora_from_sd ? kCalibrationYellow : kFieldGreen);
        obsText(c, 32, 62, s.lora_label, FontId::Title, kCalibrationYellow, 180);
        char cnt[24];
        snprintf(cnt, sizeof cnt, "%u OF %u",
                 unsigned(s.lora_index + 1), unsigned(s.lora_count ? s.lora_count : 1));
        obsMicro(c, 288, 67, cnt, kMutedSlate, kPanelDark, Align::Right);
        c.line(32, 86, 288, 86, kLineBorder);
        char freq[32], params[48];
        snprintf(freq, sizeof freq, "%lu.%03lu MHz",
                 static_cast<unsigned long>(s.lora_freq_hz / 1000000),
                 static_cast<unsigned long>((s.lora_freq_hz % 1000000) / 1000));
        snprintf(params, sizeof params, "SF%d  BW%d  CR4:%d  SYNC 0x%02X",
                 s.lora_sf, s.lora_bw_khz, s.lora_cr + 4, unsigned(s.lora_sync));
        obsText(c, 32, 94, freq, FontId::Head, kPaperPhosphor, 250);
        obsText(c, 32, 114, params, FontId::Mono, kFieldGreen, 256);
        dock_title = "LORA RX PROFILE";
        dock_desc = "BUILT-IN + /oscilla/lora_profiles.csv";
        break;
    }

    case SettingsRow::AudioCues:
        obsCard(c, 20, 40, 280, 98, s.audio_enabled ? kCalibrationYellow : kMutedSlate);
        obsMicro(c, 32, 48, "CARDPUTER SPEAKER FEEDBACK", kMutedSlate, kPanelDark);
        obsMicro(c, 288, 48, "DEFAULT MUTED", kMutedSlate, kPanelDark, Align::Right);
        obsText(c, 32, 65, s.audio_enabled ? "GEIGER ON" : "MUTED",
                FontId::Hero, s.audio_enabled ? kCalibrationYellow : kMutedSlate, 250);
        c.line(32, 102, 288, 102, kLineBorder);
        obsMicro(c, 32, 110, "SIGNAL HUNT GEIGER CADENCE (-100..-35 dBm)", kMutedSlate, kPanelDark);
        obsMicro(c, 32, 123, "HOTKEY a MUTES OR UNMUTES LIVE DURING HUNT", kDimGreen, kPanelDark);
        dock_title = "AUDIO CUES";
        dock_desc = "PERSISTED IN /oscilla/audio_enabled";
        break;

    case SettingsRow::DebugSerial:
        obsCard(c, 20, 40, 280, 98, s.debug_enabled ? kSignalPink : kMutedSlate);
        obsMicro(c, 32, 48, "USB SERIAL / JTAG CONSOLE", kMutedSlate, kPanelDark);
        obsMicro(c, 288, 48, "115200 8N1", kMutedSlate, kPanelDark, Align::Right);
        obsText(c, 32, 65, s.debug_enabled ? "ENABLED" : "DISABLED",
                FontId::Hero, s.debug_enabled ? kSignalPink : kMutedSlate, 250);
        c.line(32, 102, 288, 102, kLineBorder);
        obsMicro(c, 32, 110, "BENCH SCRIPTING & DIAGNOSTIC COMMANDS", kMutedSlate, kPanelDark);
        obsMicro(c, 32, 123, "HOTKEY d TOGGLES GLOBALLY FROM ANY CARD", kDimGreen, kPanelDark);
        dock_title = "DEBUG CONSOLE";
        dock_desc = "PERSISTED IN /oscilla/debug_mode";
        break;

    case SettingsRow::Count:
        break;
    }

    obsText(c, 12, 164, dock_title, FontId::Title, kPaperPhosphor, 230);
    char row_pos[16];
    snprintf(row_pos, sizeof row_pos, "ITEM %u/%u", unsigned(cur + 1), unsigned(kSettingsRowCount));
    obsMicro(c, 308, 168, row_pos, kMutedSlate, kPanelDark, Align::Right);
    obsMicro(c, 12, 185, dock_desc, kMutedSlate, kPanelDark);

    obsBay(c, 12, 94, "DISPLAY", std::to_string(percent) + "%", kCalibrationYellow, s.idle_label.c_str());
    obsBay(c, 122, 86, "WI-FI", s.region_code, s.region_ok && s.band_ok ? kFieldGreen : kCalibrationYellow, s.band_label.c_str());
    obsBay(c, 224, 84, "LORA", s.lora_label.substr(0, 5), kPaperPhosphor, s.lora_from_sd ? "SD" : "BIN");
}
}  // namespace ui
