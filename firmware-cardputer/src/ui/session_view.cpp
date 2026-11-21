/* Offline session archive: paired inventory, signal ribbon and raw records. */
#include "ui/session_view.h"
#include "ui/canvas.h"
#include "ui/display_text.h"
#include "ui/external_instrument.h"
#include "ui/observatory_chrome.h"
#include "ui/external_lora_view.h"
#include "ui/list_row.h"
#include "ui/lora_detail_view.h"
namespace ui {
namespace {
const char *kind(const model::SessionEntry &s) {
    return s.kind == model::SessionKind::Drive ? "WARDRIVE" : "LORA RX";
}
std::string parts(const model::SessionEntry &s) {
    std::string out = s.parts & model::Csv ? "CSV" : "NO CSV";
    if (s.parts & model::Kml) out += " / KML";
    if (s.parts & model::Manifest) out += " / META";
    if (s.parts & model::Health) out += " / HEALTH";
    return out;
}
std::string progress(const model::SessionReview &r) {
    return r.busy ? "READING " + std::to_string(r.read_bytes / 1024) + " KiB" : "FILE SNAPSHOT";
}
std::string span(const model::SessionSummary &s) {
    return s.hasSpan() ? std::to_string(s.spanMs() / 1000) + "s OBSERVED SPAN" : "TIME SPAN UNKNOWN";
}
std::string recordName(const model::SavedRecord &r, model::SessionKind k) {
    return k == model::SessionKind::Drive
               ? (r.name.empty() ? "<hidden>" : r.name)
               : std::to_string(r.packet.len) + "B / " + std::to_string(r.rssi) + "dBm";
}
std::string loraMetaText(const model::LoraSidecarSummary &m) {
    char buf[80];
    if (m.freq_hz > 0) {
        std::snprintf(buf, sizeof buf, "%lu.%03luM SF%d BW%d CR%d",
                      static_cast<unsigned long>(m.freq_hz / 1000000),
                      static_cast<unsigned long>((m.freq_hz % 1000000) / 1000),
                      m.sf, m.bw_khz, m.cr);
        return buf;
    }
    return m.profile.empty() ? "META PRESENT" : m.profile;
}
std::string loraHealthText(const model::LoraSidecarSummary &m) {
    char buf[80];
    std::snprintf(buf, sizeof buf, "RX %lu CRC %lu DRP %lu FLT %lu",
                  static_cast<unsigned long>(m.rx),
                  static_cast<unsigned long>(m.crc_err + m.header_err),
                  static_cast<unsigned long>(m.totalDrops()),
                  static_cast<unsigned long>(m.hw_fault));
    return buf;
}
} // namespace
void drawSessionView(const model::SessionReview &r, const ChromeState &chrome) {
    if (r.stage == model::ReviewStage::Hex) {
        drawLoraDetailView(r.packet, chrome, "RECORDS");
        return;
    }
    beginChrome(chrome);
    auto &d = canvas();
    d.setTextSize(1);
    auto text = [&](int y, const std::string &s, uint16_t color = kPaperPhosphor) {
        d.setCursor(4, kBodyTop + y);
        d.setTextColor(color, kVoidInk);
        d.print(printable(s, 38).c_str());
    };
    if (r.stage == model::ReviewStage::Catalog) {
        text(2,
             r.busy          ? "READING SD INVENTORY..."
             : r.filter == 1 ? "WARDRIVE SESSIONS"
             : r.filter == 2 ? "LORA SESSIONS"
                             : "ALL SESSIONS / GROUPED BY TYPE",
             kFieldGreen);
        const size_t start = listFirstVisible(r.cursor, kListCompactRows);
        for (size_t i = start; i < r.catalog.entries.size() && i < start + static_cast<size_t>(kListCompactRows); ++i) {
            const auto &e = r.catalog.entries[i];
            const bool sel = i == r.cursor;
            const int y = kListTop + int(i - start) * kListRowHeight;
            const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
            drawRowHighlight(d, y, sel);
            d.setCursor(6, y);
            d.setTextColor(sel ? kCalibrationYellow : kPaperPhosphor, bg);
            d.print(printable(model::sessionStem(e) + " / " + kind(e), 37).c_str());
        }
        if (r.catalog.entries.empty() && !r.busy)
            text(35, r.available ? "NO SAVED SESSIONS" : "SD UNAVAILABLE", kMutedSlate);
        text(92,
             r.catalog.overflow || r.limited ? "LIMITED / x TYPE / r REFRESH"
                                             : "ENTER OPEN / x TYPE / r REFRESH",
             kCalibrationYellow);
    } else if (r.stage == model::ReviewStage::Summary) {
        const auto &sc = r.summary.lora_sidecar;
        const bool lora_sidecar = r.selected.kind == model::SessionKind::Lora && (sc.has_meta || sc.has_health);
        text(2, model::sessionStem(r.selected) + " / " + kind(r.selected), kCalibrationYellow);
        text(15, parts(r.selected), kMutedSlate);
        text(28, (!r.busy && sc.has_meta) ? loraMetaText(sc) : progress(r), kFieldGreen);
        std::string rec_line = std::to_string(r.summary.valid) + " RECORDS";
        if (r.selected.kind == model::SessionKind::Lora && r.summary.geotagged)
            rec_line += " (" + std::to_string(r.summary.geotagged) + " GEO)";
        rec_line += " / " + std::to_string(r.snapshot_bytes) + " B";
        text(41, rec_line);
        text(54, span(r.summary));
        if (lora_sidecar && sc.has_health) {
            const uint16_t hcol = (sc.hw_fault || sc.totalDrops()) ? kSignalPink : kFieldGreen;
            text(67, loraHealthText(sc), hcol);
            text(79, std::to_string(r.summary.bad) + " BAD / TAIL " + (r.summary.partial ? "PARTIAL" : "COMPLETE"),
                 kMutedSlate);
        } else {
            text(73,
                 std::to_string(r.summary.bad) + " BAD / TAIL " + (r.summary.partial ? "PARTIAL" : "COMPLETE"),
                 kMutedSlate);
        }
        text(92, !r.error.empty()||!r.summary.valid?"` SESSIONS":"ENTER RECORDS / ` SESSIONS", kCalibrationYellow);
    } else {
        text(2, model::sessionStem(r.selected) + " / RECORDS", kFieldGreen);
        for (size_t i = 0; i < r.rows.size() && i < static_cast<size_t>(kListCompactRows); ++i) {
            const bool sel = i == r.record;
            const int y = kListTop + int(i) * kListRowHeight;
            const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
            drawRowHighlight(d, y, sel);
            d.setCursor(6, y);
            d.setTextColor(sel ? kCalibrationYellow : kPaperPhosphor, bg);
            d.print(printable(std::to_string(r.page_index * 6 + i + 1) + " " +
                              recordName(r.rows[i], r.selected.kind), 37).c_str());
        }
        if (r.busy) text(44, "READING RECORD PAGE...", kMutedSlate);
        text(92,
             r.selected.kind == model::SessionKind::Lora ? "; . ROW / ENTER HEX / ` SUMMARY"
                                                         : "; . ROW / ` SUMMARY",
             kCalibrationYellow);
    }
    if (!r.error.empty()) {
        const bool summary=r.stage==model::ReviewStage::Summary;
        d.fillRect(0,kBodyTop+(summary?73:17),240,summary?14:72,kVoidInk);
        text(summary?73:43,r.error,kSignalPink);
    }
    endChrome(chrome);
}
void drawExternalSessionStrip(lgfx::LGFX_Sprite &d, int top, const model::SessionReview &r) {
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    if (r.stage == model::ReviewStage::Hex) {
        drawExternalLoraDetailStrip(d, top, r.packet);
        return;
    }

    const BadgeTone tone = r.busy ? BadgeTone::Warn : BadgeTone::Cached;
    const char *badge = r.busy ? "READING" : "SNAPSHOT";

    if (r.stage == model::ReviewStage::Catalog) {
        obsFrame(c, ModeSigil::ArchiveDisk, "ARCHIVE // SESSIONS", badge, tone);
        obsDock(c, kFieldGreen, false);

        obsMicro(c, 16, 31,
                 r.filter == 1   ? "FILTER: WARDRIVE / DESC"
                 : r.filter == 2 ? "FILTER: LORA RX / DESC"
                                 : "FILTER: ALL TYPES / GROUPED DESC",
                 kDimGreen, kVoidInk);
        const size_t start = r.cursor > 3 ? r.cursor - 3 : 0;
        for (size_t i = start; i < r.catalog.entries.size() && i < start + 4; ++i) {
            const auto &e = r.catalog.entries[i];
            const int ry = 44 + int(i - start) * 25;
            const bool sel = i == r.cursor;
            if (sel) {
                c.rect(14, ry, 292, 22, kDimGreen);
                c.rect(14, ry, 3, 22, kCalibrationYellow);
            }
            obsText(c, 22, ry + 4, model::sessionStem(e), FontId::Title,
                    sel ? kCalibrationYellow : kPaperPhosphor, 196);
            obsMicro(c, 298, ry + 7, kind(e),
                     sel ? kPaperPhosphor : kMutedSlate, sel ? kDimGreen : kVoidInk, Align::Right);
        }
        if (r.catalog.entries.empty() && !r.busy) {
            obsMicro(c, 160, 86, r.available ? "NO SAVED SESSIONS" : "SD UNAVAILABLE",
                     kCalibrationYellow, kVoidInk, Align::Center);
        }

        obsText(c, 12, 164, "LOCAL RECORDINGS", FontId::Title, kFieldGreen, 296);
        obsMicro(c, 12, 185,
                 r.catalog.overflow || r.limited ? "LIMITED INVENTORY / SD SNAPSHOT"
                                                 : "OFFLINE SD ARCHIVE / READ ONLY",
                 kMutedSlate, kPanelDark);
        obsBay(c, 12, 130, "SESSIONS", std::to_string(r.catalog.entries.size()), kCalibrationYellow);
        obsBay(c, 168, 136, "STORAGE", r.available ? "SD READY" : "ABSENT",
               r.available ? kFieldGreen : kCalibrationYellow);
    } else if (r.stage == model::ReviewStage::Summary) {
        obsFrame(c, ModeSigil::ArchiveDisk, "ARCHIVE // SUMMARY", badge, tone);
        obsDock(c, kCalibrationYellow, true);

        const auto &s = r.summary;
        constexpr int x0 = 38, y0 = 46, w = 146, h = 80;
        for (int i = 0; i <= 4; ++i) {
            const int yy = y0 + (h * i) / 4;
            if (i == 4) c.line(x0, yy, x0 + w, yy, kDimGreen);
            else obsDashedH(c, x0, x0 + w, yy, kTrackDark, 2, 3);
        }
        obsMicro(c, 35, y0 - 3, "-20", kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, 35, y0 + h - 4, "-120", kMutedSlate, kVoidInk, Align::Right);
        obsMicro(c, x0, 32, "LAST 32 VALID / RSSI", kDimGreen, kVoidInk);

        for (size_t i = 0; i < s.signal_count; ++i) {
            const int x = x0 + int(i) * w / 31;
            const int y = y0 + h - (std::max(-120, std::min(-20, s.signals[i])) + 120) * h / 100;
            c.line(x, y + 2, x, y0 + h - 1, kTrackDark);
            if (i) {
                const int px = x0 + int(i - 1) * w / 31;
                const int py = y0 + h - (std::max(-120, std::min(-20, s.signals[i - 1])) + 120) * h / 100;
                c.line(px, py, x, y, kFieldGreen);
            }
            c.rect(x - 1, y - 1, 2, 2, kCalibrationYellow);
        }
        obsMicro(c, x0, 134, span(s), kMutedSlate, kVoidInk);

        obsCard(c, 194, 36, 110, 103, kCalibrationYellow);
        obsMicro(c, 202, 42, "RECORDS", kMutedSlate, kPanelDark);
        obsText(c, 202, 54, std::to_string(s.valid), FontId::Hero, kPaperPhosphor, 94);
        c.line(202, 78, 296, 78, kLineBorder);
        obsMicro(c, 202, 85, "BAD ROWS", kMutedSlate, kPanelDark);
        obsText(c, 202, 96, std::to_string(s.bad), FontId::Title, s.bad ? kSignalPink : kFieldGreen, 94);
        obsMicro(c, 202, 121, s.partial ? "PARTIAL TAIL" : "COMPLETE",
                 s.partial ? kCalibrationYellow : kMutedSlate, kPanelDark);

        obsText(c, 12, 164, model::sessionStem(r.selected), FontId::Title, kCalibrationYellow, 210);
        obsBadge(c, 232, 164, 76, 15, kind(r.selected), kTrackDark, kDimGreen, kFieldGreen);
        const auto &sc = s.lora_sidecar;
        const std::string sub_left =
            (r.selected.kind == model::SessionKind::Lora && sc.has_health) ? loraHealthText(sc) : parts(r.selected);
        const std::string sub_right =
            (r.selected.kind == model::SessionKind::Lora && !r.busy && sc.has_meta) ? loraMetaText(sc) : progress(r);
        obsMicro(c, 12, 185, sub_left,
                 (sc.has_health && (sc.hw_fault || sc.totalDrops())) ? kSignalPink : kMutedSlate, kPanelDark);
        obsMicro(c, 308, 185, sub_right, kFieldGreen, kPanelDark, Align::Right);

        obsBay(c, 12, 94, "VALID", std::to_string(s.valid), kPaperPhosphor);
        obsBay(c, 122, 86, "BAD", std::to_string(s.bad), s.bad ? kSignalPink : kMutedSlate);
        obsBay(c, 224, 84, "SIZE", std::to_string(r.snapshot_bytes / 1024), kFieldGreen, "KB");
    } else {
        obsFrame(c, ModeSigil::ArchiveDisk, "ARCHIVE // RECORD", badge, tone);
        obsDock(c, kFieldGreen, true);

        if (r.record < r.rows.size()) {
            const auto &row = r.rows[r.record];
            obsCard(c, 16, 36, 288, 103, kCalibrationYellow);
            obsMicro(c, 26, 42,
                     "RECORD " + std::to_string(r.page_index * 6 + r.record + 1) + " / " +
                         std::to_string(r.summary.valid),
                     kMutedSlate, kPanelDark);
            obsMicro(c, 294, 42, kind(r.selected), kFieldGreen, kPanelDark, Align::Right);
            obsText(c, 26, 55, recordName(row, r.selected.kind), FontId::Title, kPaperPhosphor, 268);
            c.line(26, 77, 294, 77, kLineBorder);
            obsMicro(c, 26, 84,
                     r.selected.kind == model::SessionKind::Drive
                         ? row.identity
                         : (std::to_string(row.frequency) + " Hz / SF" + std::to_string(row.sf) +
                            " / BW" + std::to_string(row.bw)),
                     kCalibrationYellow, kPanelDark);
            obsMicro(c, 26, 99, row.stamp, kMutedSlate, kPanelDark);
            const int rssi_pct = std::max(0, std::min(100, (row.rssi + 120) * 100 / 90));
            obsSegBar(c, 26, 118, 268, 10, 20, rssi_pct, kFieldGreen);

            obsText(c, 12, 164, model::sessionStem(r.selected), FontId::Title, kPaperPhosphor, 210);
            obsMicro(c, 12, 185,
                     r.selected.kind == model::SessionKind::Drive
                         ? row.auth
                         : obsUpper(std::string("FRAMING GUESS / ") + model::framingName(row.packet.framing)),
                     kMutedSlate, kPanelDark);
            obsBay(c, 12, 94, "RSSI / dBm", std::to_string(row.rssi), kCalibrationYellow);
            obsBay(c, 122, 86, "RECORD", std::to_string(r.page_index * 6 + r.record + 1), kPaperPhosphor);
            obsBay(c, 224, 84, "STATE", "SAVED", kMutedSlate);
        } else {
            obsMicro(c, 160, 86, r.busy ? "READING RECORD PAGE..." : "NO VALID RECORDS",
                     kMutedSlate, kVoidInk, Align::Center);
            obsText(c, 12, 164, model::sessionStem(r.selected), FontId::Title, kPaperPhosphor, 296);
            obsBay(c, 12, 94, "RSSI / dBm", "--", kCalibrationYellow);
            obsBay(c, 122, 86, "RECORD", "--", kPaperPhosphor);
            obsBay(c, 224, 84, "STATE", "EMPTY", kMutedSlate);
        }
    }

    if (!r.error.empty()) {
        c.rect(8, 226, 304, 13, kPanelDark);
        obsMicro(c, 12, 229, r.error, kSignalPink, kPanelDark);
    }
}
} // namespace ui
