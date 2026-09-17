/* mesh_view.cpp — see mesh_view.h. */
#include "ui/mesh_view.h"
#include <M5Cardputer.h>
#include "ui/canvas.h"
#include "ui/link_view.h"
namespace ui {
void drawMeshView(const model::MeshModel &m, size_t cursor, const std::string &notice)
{
    auto &d = canvas(); d.fillScreen(TFT_BLACK); d.setTextSize(1); d.setCursor(0, 0); d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.printf("MESH 154 %s", m.active() ? "RX" : "idle"); d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.printf("  pans %u nodes %u", (unsigned)m.pans().size(), (unsigned)m.nodes().size());
    int y = 13; const int h = 11; size_t first = cursor > 8 ? cursor - 8 : 0;
    for (size_t i = first; i < m.nodes().size() && y < d.height() - 22; ++i, y += h) {
        const auto &n = m.nodes()[i]; bool sel = i == cursor; if (sel) d.fillRect(0, y - 1, d.width(), h, TFT_NAVY);
        d.setTextColor(sel ? TFT_WHITE : TFT_LIGHTGREY, sel ? TFT_NAVY : TFT_BLACK);
        d.printf("%s %s %4d L%u", n.pan.c_str(), n.short_addr.empty() ? "--------" : n.short_addr.c_str(), n.rssi, n.lqi);
    }
    d.setTextColor(TFT_YELLOW, TFT_BLACK); d.setCursor(0, d.height() - 20); d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK); d.setCursor(0, d.height() - 10); d.print(";. move  ,/ cards  s start/stop");
}
}  // namespace ui
