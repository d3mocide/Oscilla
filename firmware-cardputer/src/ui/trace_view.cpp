/*
 * trace_view.cpp — see trace_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/trace_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/theme.h"

namespace ui {

namespace {

uint16_t rssiColour(int rssi)
{
    if (rssi >= -55) return kFieldGreen;
    if (rssi >= -70) return kCalibrationYellow;
    return kFaultRed;
}

void metric(M5Canvas &d, int x, const char *name, const char *value, const char *sub,
            uint16_t value_color)
{
    d.drawRect(x, kBodyTop + 42, 75, 25, kLineBorder);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(x + 4, kBodyTop + 45);
    d.print(name);
    d.setTextColor(value_color, kVoidInk);
    d.setCursor(x + 4, kBodyTop + 54);
    d.print(value);
    d.setTextColor(kDimGreen, kVoidInk);
    d.setCursor(x + 4, kBodyTop + 63);
    d.print(sub);
}

}  // namespace

void drawTraceView(const model::ApRow *row, const model::Inspect &in, bool listening,
                   const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    if (!row) {
        d.setCursor(4, kBodyTop + 4);
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("NO TARGET · RETURN TO WIFI SCAN");
    } else {
        std::string name = row->ssid.empty() ? "<hidden>" : printable(row->ssid, 18);
        d.setTextSize(2);
        d.setCursor(3, kBodyTop + 1);
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.printf("> %s", name.c_str());
        d.setTextSize(1);

        d.setCursor(4, kBodyTop + 27);
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf("CH %u · %s GHz", row->ch, row->band5 ? "5" : "2.4");
        d.setCursor(132, kBodyTop + 27);
        d.setTextColor(rssiColour(row->rssi), kVoidInk);
        d.printf("RSSI %d dBm", row->rssi);

        if (listening) {
            d.setCursor(4, kBodyTop + 83);
            d.setTextColor(kCalibrationYellow, kVoidInk);
            d.printf("CAPTURE ACTIVE · CH %u", row->ch);
        } else if (!in.valid) {
            metric(d, 3, "SECURITY", printable(row->auth, 10).c_str(), "NOT INSPECTED", kPaperPhosphor);
            metric(d, 82, "MFP", "--", "PRESS ENTER", kMutedSlate);
            metric(d, 161, "BEACONS", "--", "PRESS ENTER", kMutedSlate);
        } else if (in.beacons == 0) {
            metric(d, 3, "SECURITY", printable(row->auth, 10).c_str(), "NO BEACONS", kPaperPhosphor);
            metric(d, 82, "MFP", "--", "NO DATA", kCalibrationYellow);
            metric(d, 161, "BEACONS", "0", in.aborted ? "STOPPED" : "NONE HEARD", kCalibrationYellow);
        } else {
            const char *mfp = !in.rsn ? "NO RSN" : in.mfp_required ? "REQ" : in.mfp_capable ? "CAP" : "OFF";
            uint16_t mfp_color = in.mfp_required ? kFieldGreen : in.mfp_capable ? kCalibrationYellow : kSignalPink;
            char beacons[16];
            std::snprintf(beacons, sizeof beacons, "%u", in.beacons);
            char interval[16];
            std::snprintf(interval, sizeof interval, "%ums", in.interval_ms);
            metric(d, 3, "SECURITY", printable(row->auth, 10).c_str(), "AUTH MODE", kPaperPhosphor);
            metric(d, 82, "MFP", mfp, interval, mfp_color);
            metric(d, 161, "BEACONS", beacons, "CAPTURED", kFieldGreen);
            d.setCursor(4, kBodyTop + 83);
            d.setTextColor(mfp_color, kVoidInk);
            d.print(in.mfp_required ? "◇ MFP REQUIRED" : in.mfp_capable ? "◇ MFP CAPABLE" : "◇ MFP NOT OBSERVED");
        }
    }

    endChrome(chrome);
}

}  // namespace ui
