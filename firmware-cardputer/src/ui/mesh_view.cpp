/* mesh_view.cpp — see mesh_view.h. */
#include "ui/mesh_view.h"
#include <M5Cardputer.h>
#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/theme.h"
namespace ui {
void drawMeshView(const model::MeshModel &m, size_t cursor, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);
    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("PANS %u  NODES %u  %s", (unsigned)m.pans().size(), (unsigned)m.nodes().size(),
             m.active() ? "LISTENING" : "IDLE");

    constexpr int list_top = kBodyTop + 16;
    constexpr int row_height = 13;
    constexpr int visible = 4;
    size_t first = cursor >= static_cast<size_t>(visible) ? cursor - visible + 1 : 0;
    for (int row = 0; row < visible && first + static_cast<size_t>(row) < m.nodes().size(); ++row) {
        const auto &n = m.nodes()[first + static_cast<size_t>(row)];
        const bool sel = first + static_cast<size_t>(row) == cursor;
        const int y = list_top + row * row_height;
        const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        if (sel) {
            d.fillRect(0, y - 2, d.width(), row_height, bg);
            d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);
        }
        d.setCursor(6, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%-8s", printable(n.pan, 8).c_str());
        const std::string short_addr = n.short_addr.empty() ? "--------" : printable(n.short_addr, 8);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" %-8s", short_addr.c_str());
        d.setTextColor(n.rssi >= -70 ? kFieldGreen : kCalibrationYellow, bg);
        d.printf(" %4d", n.rssi);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" L%u", n.lqi);
    }
    if (m.nodes().empty()) {
        d.setCursor(4, list_top);
        d.setTextColor(kDimGreen, kVoidInk);
        d.print(m.active() ? "LISTENING FOR PAN/NODE FRAMES..." : "NO 802.15.4 OBSERVATIONS YET");
    }
    d.setCursor(4, kBodyTop + 83);
    if (!m.nodes().empty() && cursor < m.nodes().size()) {
        const auto &n = m.nodes()[cursor];
        d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("%s RSSI %d LQI %u", printable(n.pan, 8).c_str(), n.rssi, n.lqi);
    } else {
        d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("NODES %u · %s", (unsigned)m.nodes().size(), m.active() ? "LISTENING" : "IDLE");
    }

    endChrome(chrome);
}
}  // namespace ui
