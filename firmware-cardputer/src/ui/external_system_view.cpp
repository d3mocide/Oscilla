/* Startup mark, Grove link health, and 60s deck/probe heap monitor; Observatory v2. */
#include "ui/external_system_view.h"
#include "ui/observatory_chrome.h"
#include <cmath>
#include <cstdio>

namespace ui {
namespace {
/* Full Lens Core mark (docs/brand/README.md): green hex/orbits, cream lens, yellow core; no pink. */
void drawLensCore(lgfx::LGFX_Sprite &d, int cx, int cy, int top)
{
    auto line = [&](int x, int y, int xx, int yy, uint16_t c) {
        d.drawLine(cx + x, cy + y - top, cx + xx, cy + yy - top, c);
    };
    const int hex[][2] = {{0, -40}, {35, -20}, {35, 20}, {0, 40}, {-35, 20}, {-35, -20}};
    for (int i = 0; i < 6; ++i) {
        line(hex[i][0], hex[i][1], hex[(i + 1) % 6][0], hex[(i + 1) % 6][1], kFieldGreen);
    }
    for (int axis = 0; axis < 2; ++axis) {
        int lastx = 0, lasty = 0;
        const uint16_t oc = axis ? kDimGreen : kFieldGreen;
        for (int i = 0; i <= 48; ++i) {
            const double a = i * 6.283185307179586 / 48;
            const int x = int(std::lround(std::cos(a) * (axis ? 13 : 33)));
            const int y = int(std::lround(std::sin(a) * (axis ? 33 : 13)));
            if (i) line(lastx, lasty, x, y, oc);
            lastx = x; lasty = y;
        }
    }
    const int lens[][2] = {{-16, 0}, {-10, -7}, {0, -9}, {10, -7}, {16, 0}, {10, 7}, {0, 9}, {-10, 7}};
    for (int i = 0; i < 8; ++i) {
        line(lens[i][0], lens[i][1], lens[(i + 1) % 8][0], lens[(i + 1) % 8][1], kPaperPhosphor);
    }
    d.fillCircle(cx, cy - top, 5, kCalibrationYellow);
}
}  // namespace

void drawExternalStartupStrip(lgfx::LGFX_Sprite &d, int top)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    c.clear();
    c.line(14, 12, 306, 12, kDimGreen);
    c.line(14, 228, 306, 228, kDimGreen);
    c.corners(12, 12, 296, 216, kFieldGreen);
    drawLensCore(d, 160, 86, top);
    obsText(c, 160, 142, "OSCILLA", FontId::Hero, kPaperPhosphor, 260, Align::Center);
    obsText(c, 160, 174, "OBSERVE THE NOISE.", FontId::Head, kFieldGreen, 280, Align::Center);
    obsMicro(c, 160, 202, "PASSIVE RF OBSERVATORY / VIEWPORT READY", kMutedSlate, kVoidInk, Align::Center);
}

void drawExternalSystemStrip(lgfx::LGFX_Sprite &d, int top, const model::SystemStatus &s,
                             uint32_t now, const model::SystemResources *resources)
{
    using namespace instrument;
    using namespace observatory;
    const bool ready = s.link == ocp::LinkState::Ready;
    const char *state = ready ? "READY"
                      : s.link == ocp::LinkState::HelloSent ? "SYNCING"
                      : s.link == ocp::LinkState::Incompatible ? "MISMATCH" : "OFFLINE";
    const BadgeTone tone = ready ? BadgeTone::Live
                         : s.link == ocp::LinkState::Incompatible ? BadgeTone::Fault : BadgeTone::Warn;
    Canvas c(d, top);

    if (s.info) {
        obsFrame(c, ModeSigil::SystemCore, "SYSTEM // HEAP FREE %", state, tone);
        obsDock(c, ready ? kFieldGreen : kCalibrationYellow, false);

        const bool fresh = s.probe_valid && ready && now - s.probe_sample_ms <= 6000;
        obsMicro(c, 18, 31, "DECK " + std::to_string(s.deck_heap / 1024) + " KB", kFieldGreen, kVoidInk);
        obsMicro(c, 302, 31,
                 s.probe_valid ? ("PROBE " + std::to_string(s.probe_heap / 1024) + " KB") : "PROBE / NO SAMPLE",
                 fresh ? kSignalPink : kMutedSlate, kVoidInk, Align::Right);

        constexpr int x0 = 36, y0 = 48, w = 264, h = 82;
        for (int i = 0; i <= 4; ++i) {
            const int x = x0 + (w * i) / 4;
            obsDashedV(c, x, y0, y0 + h, kTrackDark, 2, 3);
        }
        for (int i = 0; i <= 2; ++i) {
            const int y = y0 + (h * i) / 2;
            if (i == 2) c.line(x0, y, x0 + w, y, kDimGreen);
            else obsDashedH(c, x0, x0 + w, y, kTrackDark, 2, 3);
        }

        unsigned low = 10000, high = 0;
        bool any = false;
        if (resources) {
            for (const auto *history : {&resources->deck(), &resources->probe()}) {
                model::visitHeapPoints(*history, now, 4500, [&](const model::HeapPoint &pt) {
                    low = std::min(low, pt.basis_points);
                    high = std::max(high, pt.basis_points);
                    any = true;
                });
            }
        }
        const auto scale = model::heapScaleBounds(low, high, any);
        auto mapY = [&](unsigned bp) {
            if (scale.high <= scale.low) return y0 + h / 2;
            const unsigned clamped = std::max(scale.low, std::min(scale.high, bp));
            return y0 + h - int(uint64_t(clamped - scale.low) * h / (scale.high - scale.low));
        };
        auto tick = [&](int y, unsigned value) {
            char label[16];
            if (value % 100) snprintf(label, sizeof label, "%u.%u", value / 100, (value % 100) / 10);
            else snprintf(label, sizeof label, "%u", value / 100);
            obsMicro(c, 32, y, label, kMutedSlate, kVoidInk, Align::Right);
        };
        tick(y0 - 3, scale.high);
        tick(y0 + h / 2 - 3, (scale.low + scale.high) / 2);
        tick(y0 + h - 4, scale.low);
        obsMicro(c, x0, 136, "-60s", kMutedSlate, kVoidInk);
        obsMicro(c, x0 + w / 2, 136, "-30s / AUTO SCALE", kMutedSlate, kVoidInk, Align::Center);
        obsMicro(c, x0 + w, 136, "NOW", kCalibrationYellow, kVoidInk, Align::Right);

        if (resources) {
            auto trace = [&](const model::HeapHistory &history, uint32_t max_gap, uint16_t color) {
                int px = 0, py = 0;
                bool have = false;
                model::visitHeapPoints(history, now, max_gap, [&](const model::HeapPoint &pt) {
                    const int x = x0 + w - int(uint64_t(pt.age) * w / 60000);
                    const int y = mapY(pt.basis_points);
                    if (pt.connected) c.line(px, py, x, y, color);
                    else c.dot(x, y, 1, color);
                    px = x; py = y; have = true;
                });
                if (have) c.dot(px, py, 2, color);
            };
            trace(resources->deck(), 1600, kFieldGreen);
            trace(resources->probe(), 4500, fresh ? kSignalPink : kMutedSlate);
        }

        /* Compact 2x2 live subsystem status grid in Reading Dock. */
        obsText(c, 12, 164, "SUBSYSTEM ACTIVITY", FontId::Head, kPaperPhosphor, 175);
        obsMicro(c, 308, 166,
                 !s.probe_valid ? "WAITING FOR PROBE"
                                : (std::string(fresh ? "PROBE " : "STALE ") +
                                   std::to_string((now - s.probe_sample_ms) / 1000) + "s AGO"),
                 fresh ? kMutedSlate : kCalibrationYellow, kPanelDark, Align::Right);
        obsMicro(c, 12, 185, "DECK GREEN / PROBE PINK / AUTO % AXIS", kMutedSlate, kPanelDark);

        const uint16_t radio_col = fresh ? kPaperPhosphor : kMutedSlate;
        const std::string lora = obsUpper(s.probe_lora);
        const std::string gnss_lbl = obsUpper(model::gnssStateName(s.gnss_state));
        const char *log_lbl = s.drive_logging && s.lora_logging ? "DRIVE + LORA"
                            : s.drive_logging ? "DRIVE"
                            : s.lora_logging ? "LORA"
                            : s.sd_ready ? "IDLE" : "SD ABSENT";
        c.line(160, 202, 160, 235, kLineBorder);
        obsMicro(c, 12, 205, "PHY", kMutedSlate, kPanelDark);
        obsMicro(c, 48, 205, fresh ? obsUpper(s.phy_task) : "UNKNOWN", radio_col, kPanelDark);
        obsMicro(c, 12, 222, "LORA", kMutedSlate, kPanelDark);
        obsMicro(c, 48, 222, fresh && !lora.empty() ? lora : "UNKNOWN",
                 fresh && s.probe_lora == OCP_K_RX ? kFieldGreen : radio_col, kPanelDark);
        obsMicro(c, 170, 205, "GNSS", kMutedSlate, kPanelDark);
        obsMicro(c, 210, 205, gnss_lbl,
                 s.gnss_state == model::GnssState::Fixed ? kFieldGreen : kCalibrationYellow, kPanelDark);
        obsMicro(c, 170, 222, "LOG", kMutedSlate, kPanelDark);
        obsMicro(c, 210, 222, log_lbl,
                 s.drive_logging || s.lora_logging ? kFieldGreen : kMutedSlate, kPanelDark);
        return;
    }

    /* Link overview view. */
    obsFrame(c, ModeSigil::SystemCore, "DECK // PROBE LINK", state, tone);
    obsDock(c, ready ? kFieldGreen : kCalibrationYellow, true);

    drawLensCore(d, 74, 88, top);

    obsCard(c, 134, 36, 170, 103, ready ? kFieldGreen : kCalibrationYellow);
    obsMicro(c, 144, 42, "PROBE LINK", kMutedSlate, kPanelDark);
    obsText(c, 144, 54, state, FontId::Title, ready ? kFieldGreen : kCalibrationYellow, 152);
    c.line(144, 75, 296, 75, kLineBorder);
    obsMicro(c, 144, 82, "GNSS RECEIVER", kMutedSlate, kPanelDark);
    const std::string gnss_lbl = obsUpper(model::gnssStateName(s.gnss_state));
    obsText(c, 144, 94, gnss_lbl, FontId::Title,
            s.gnss_state == model::GnssState::Fixed ? kFieldGreen : kCalibrationYellow, 152);
    obsMicro(c, 144, 121, ready ? "GROVE UART / OCP v1" : "LINK UNSYNCHRONIZED", kMutedSlate, kPanelDark);

    obsText(c, 12, 164, ready ? "LINK SYNCHRONIZED" : "AWAITING PROBE",
            FontId::Title, ready ? kFieldGreen : kCalibrationYellow, 296);
    char stats_buf[48];
    snprintf(stats_buf, sizeof stats_buf, "ERR %u   TIMEOUT %u   RESET %u",
             unsigned(s.stats.errors), unsigned(s.stats.timeouts), unsigned(s.stats.resets));
    obsMicro(c, 12, 185, stats_buf,
             s.stats.errors || s.stats.timeouts ? kCalibrationYellow : kMutedSlate, kPanelDark);

    obsBay(c, 12, 94, "SD CARD", s.sd_ready ? "READY" : "ABSENT",
           s.sd_ready ? kFieldGreen : kCalibrationYellow);
    obsBay(c, 122, 86, "BUS", ready ? "OCP" : "WAIT", ready ? kPaperPhosphor : kMutedSlate,
           ready ? "UART" : "");
    obsBay(c, 224, 84, "FAULTS", std::to_string(unsigned(s.stats.errors + s.stats.timeouts)),
           (s.stats.errors || s.stats.timeouts) ? kSignalPink : kFieldGreen);
}
}  // namespace ui
