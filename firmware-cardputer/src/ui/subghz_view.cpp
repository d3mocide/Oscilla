/*
 * subghz_view.cpp — see subghz_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/subghz_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

namespace {

uint16_t snrColour(float snr)
{
    if (snr >= 7.0f) return kFieldGreen;
    if (snr >= 0.0f) return kCalibrationYellow;
    return kSignalPink;   /* decoded, but weak/marginal */
}

/* One-letter framing_guess tag (model/lora_framing.h): a guess, so kept
 * visually secondary to RSSI/SNR, not asserted as a decode. */
char framingChar(model::Framing f)
{
    switch (f) {
    case model::Framing::Meshtastic: return 'M';
    case model::Framing::LoRaWAN: return 'L';
    case model::Framing::MeshCore: return 'C';
    default: return '-';
    }
}

uint16_t framingColour(model::Framing f)
{
    switch (f) {
    case model::Framing::Meshtastic: return kSignalPink;
    case model::Framing::MeshCore: return kFieldGreen;
    case model::Framing::LoRaWAN: return kPaperPhosphor;
    default: return kMutedSlate;
    }
}

}  // namespace

void drawSubGhzView(const model::LoraModel &lora, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("PACKETS %u  %s", (unsigned)lora.totalCount(), lora.active() ? "LISTENING" : "IDLE");
    if (lora.hasConfig()) {
        d.setCursor(184, kBodyTop + 2);
        d.printf("SF%d BW%d", lora.sf(), lora.bwKhz());
    } else {
        d.setCursor(164, kBodyTop + 2);
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("UNCONFIGURED");
    }

    const auto &packets = lora.packets();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + static_cast<size_t>(i) < packets.size(); i++) {
        const auto &p = packets[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        d.setCursor(6, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%4d", p.rssi);
        d.setTextColor(snrColour(p.snr), bg);
        d.printf(" %5.1f", static_cast<double>(p.snr));
        d.setTextColor(kMutedSlate, bg);
        d.printf(" %3u ", p.len);
        d.setTextColor(framingColour(p.framing), bg);
        d.printf("%c ", framingChar(p.framing));
        d.setTextColor(kDimGreen, bg);
        d.print(printable(p.hex, 20).c_str());
    }

    if (packets.empty()) {
        d.setTextColor(kDimGreen, kVoidInk);
        d.setCursor(0, kListTop);
        d.print(lora.active() ? "LISTENING FOR LORA RX..." : lora.hasConfig() ? "PRESS s TO START" : "PRESS c TO CONFIGURE");
    }

    d.setCursor(4, kDetailRowY);
    if (lora.hasConfig()) {
        char freq[16];
        std::snprintf(freq, sizeof freq, "%lu.%03lu",
                      (unsigned long)(lora.freqHz() / 1000000UL),
                      (unsigned long)((lora.freqHz() % 1000000UL) / 1000UL));
        d.setTextColor(lora.active() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("%s SF%d BW%d %s", freq, lora.sf(), lora.bwKhz(),
                 lora.profile() == "meshcore_us_ca" ? "MCORE" : "MANUAL");
        if (lora.health().valid) {
            d.setCursor(4, kDetailRowY + 9);
            d.setTextColor((lora.health().irq_drop || lora.health().radio_drop ||
                            lora.health().ocp_drop || lora.health().hw_fault)
                               ? kCalibrationYellow : kMutedSlate, kVoidInk);
            d.printf("RX%u C%u H%u D%u/%u/%u F%u", (unsigned)lora.health().rx,
                     (unsigned)lora.health().crc_err, (unsigned)lora.health().header_err,
                     (unsigned)lora.health().irq_drop, (unsigned)lora.health().radio_drop,
                     (unsigned)lora.health().ocp_drop, (unsigned)lora.health().hw_fault);
        }
    } else {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("CFG REQUIRED · PRESS c");
    }

    endChrome(chrome);
}

}  // namespace ui
