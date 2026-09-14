/*
 * subghz_view.cpp — see subghz_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/subghz_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"

namespace ui {

namespace {

constexpr int kLineH = 10;

uint16_t snrColour(float snr)
{
    if (snr >= 7.0f) return TFT_GREEN;
    if (snr >= 0.0f) return TFT_YELLOW;
    return TFT_ORANGE;   /* still decoded (it passed CRC), just a weak/marginal link */
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
    case model::Framing::Meshtastic: return TFT_MAGENTA;
    case model::Framing::MeshCore: return TFT_GREEN;
    case model::Framing::LoRaWAN: return TFT_CYAN;
    default: return TFT_DARKGREY;
    }
}

}  // namespace

void drawSubGhzView(const model::LoraModel &lora, size_t cursor, const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("SUBGHZ ");
    d.setTextColor(lora.active() ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    d.print(lora.active() ? "listening " : "stopped ");
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    if (lora.hasConfig()) {
        d.printf("%lu sf%d bw%d n=%u\n", (unsigned long)lora.freqHz(), lora.sf(), lora.bwKhz(),
                 (unsigned)lora.totalCount());
    } else {
        d.print("unconfigured\n");
    }

    const auto &packets = lora.packets();
    int list_top = kLineH + 2;
    int visible = (d.height() - list_top - 2 * kLineH) / kLineH;
    size_t first = cursor >= static_cast<size_t>(visible) ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + static_cast<size_t>(i) < packets.size(); i++) {
        const auto &p = packets[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = list_top + i * kLineH;
        if (sel) d.fillRect(0, y - 1, d.width(), kLineH, TFT_NAVY);
        uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;

        d.setCursor(0, y);
        d.setTextColor(TFT_LIGHTGREY, bg);
        d.printf("%4d", p.rssi);
        d.setTextColor(snrColour(p.snr), bg);
        d.printf(" %5.1f", static_cast<double>(p.snr));
        d.setTextColor(TFT_LIGHTGREY, bg);
        d.printf(" %3u ", p.len);
        d.setTextColor(framingColour(p.framing), bg);
        d.printf("%c ", framingChar(p.framing));
        d.setTextColor(TFT_DARKGREY, bg);
        d.print(printable(p.hex, 20).c_str());
    }

    if (packets.empty()) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.setCursor(0, list_top);
        d.print(lora.active() ? "listening..." : lora.hasConfig() ? "s to start" : "c to configure");
    }

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(";. move  c config  s start/stop  ` back");
}

}  // namespace ui
