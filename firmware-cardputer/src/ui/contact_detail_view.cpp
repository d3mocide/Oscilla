/* Full selected identities; Observatory v2 deck detail card. */
#include "ui/contact_detail_view.h"
#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/observatory_chrome.h"
#include "ui/theme.h"

namespace ui {
void drawContactDetailView(const model::ContactInspection &p, bool live, uint32_t now, const ChromeState &chrome)
{
    beginChrome(chrome);
    auto &d = canvas();
    const auto &key = p.key();
    instrument::Canvas c(d, 0);

    d.fillRect(0, kBodyTop, d.width(), 20, kPanelDark);
    d.fillRect(0, kBodyTop, 2, 20, kCalibrationYellow);
    d.drawFastHLine(0, kBodyTop + 20, d.width(), kLineBorder);
    observatory::obsText(c, 6, kBodyTop + 4, printable(key.mac, 17),
                         observatory::FontId::Title, kPaperPhosphor, 230);

    d.setTextSize(1);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(5, kBodyTop + 24);
    d.print(key.probes ? "REQUESTED SSID / NOT ASSOCIATION" : "OBSERVED AP LINK");
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.setCursor(5, kBodyTop + 36);
    d.print(printable(key.target.empty() ? "<wildcard>" : key.target, 37).c_str());
    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(5, kBodyTop + 49);
    if (key.probes) d.print("PASSIVE PROBE REQUEST");
    else if (!p.cachedApName().empty()) d.printf("SCAN NAME: %.26s", printable(p.cachedApName(), 26).c_str());
    else d.printf("CH %u / %s", p.client().ch, p.client().band5 ? "5 GHz" : "2.4 GHz");
    d.setCursor(5, kBodyTop + 62);
    d.setTextColor(kFieldGreen, kVoidInk);
    if (key.probes) d.printf("%d dBm / PKTS %lu", p.rssi(), (unsigned long)p.packets());
    else d.printf("%ddBm CH%u %s PKTS%lu", p.rssi(), p.client().ch, p.client().band5 ? "5G" : "2.4G", (unsigned long)p.packets());
    d.setCursor(5, kBodyTop + 76);
    d.setTextColor(p.present() && now - p.sampleMs() < 5000 ? kMutedSlate : kCalibrationYellow, kVoidInk);
    d.printf("%s / SNAPSHOT %lus", p.stateLabel(live, now), (unsigned long)((now - p.sampleMs()) / 1000));
    d.setCursor(5, kBodyTop + 90);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print(live ? "` LIST / s PAUSE RX" : "` LIST / s RESTART RX");
    endChrome(chrome);
}
}  // namespace ui
