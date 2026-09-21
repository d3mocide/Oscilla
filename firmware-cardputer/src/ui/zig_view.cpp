/* zig_view.cpp — see zig_view.h. */
#include "ui/zig_view.h"
#include <cstdlib>
#include <string>
#include <M5Cardputer.h>
#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"
namespace ui {

namespace {

constexpr int kZigNodesX = 76;
constexpr int kZigStatusX = 150;

/* short_addr is the common case; an extended-address-only node (802.15.4
 * src addressing mode 3) has no short address at all, so falling straight
 * to "--------" made that node's identity invisible on screen. Show a
 * truncated, visually distinct form here; the detail line shows it in full. */
std::string addrColumn(const model::ZigNode &n, bool *is_ext)
{
    *is_ext = false;
    if (!n.short_addr.empty()) return printable(n.short_addr, 8);
    if (!n.ext.empty()) {
        *is_ext = true;
        return n.ext.size() > 8 ? printable(n.ext.substr(n.ext.size() - 8), 8) : printable(n.ext, 8);
    }
    return "--------";
}

int channelCount(const std::string &hex)
{
    uint16_t bits = (uint16_t)std::strtoul(hex.c_str(), nullptr, 16);
    int n = 0;
    while (bits) { n += bits & 1; bits >>= 1; }
    return n;
}

void drawNodeRows(const model::ZigModel &m, size_t cursor)
{
    auto &d = canvas();
    const auto &nodes = m.nodes();
    size_t first = listFirstVisible(cursor);
    for (int row = 0; row < kListVisibleRows && first + static_cast<size_t>(row) < nodes.size(); ++row) {
        const auto &n = nodes[first + static_cast<size_t>(row)];
        const bool sel = first + static_cast<size_t>(row) == cursor;
        const int y = kListTop + row * kListRowHeight;
        const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);
        d.setCursor(6, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%-8s", printable(n.pan, 8).c_str());
        bool is_ext = false;
        const std::string addr = addrColumn(n, &is_ext);
        d.setTextColor(is_ext ? kSignalPink : kMutedSlate, bg);
        d.printf(" %-8s", addr.c_str());
        d.setTextColor(n.rssi >= -70 ? kFieldGreen : kCalibrationYellow, bg);
        d.printf(" %4d", n.rssi);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" L%u", n.lqi);
    }
    if (nodes.empty()) {
        d.setCursor(4, kListTop);
        d.setTextColor(kDimGreen, kVoidInk);
        d.print(m.active() ? "LISTENING FOR PAN/NODE FRAMES..." : "NO 802.15.4 OBSERVATIONS YET");
    }
}

void drawPanRows(const model::ZigModel &m, size_t cursor)
{
    auto &d = canvas();
    const auto &pans = m.pans();
    size_t first = listFirstVisible(cursor);
    for (int row = 0; row < kListVisibleRows && first + static_cast<size_t>(row) < pans.size(); ++row) {
        const auto &p = pans[first + static_cast<size_t>(row)];
        const bool sel = first + static_cast<size_t>(row) == cursor;
        const int y = kListTop + row * kListRowHeight;
        const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);
        d.setCursor(6, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%-8s", printable(p.pan, 8).c_str());
        d.setTextColor(kMutedSlate, bg);
        d.printf(" N%-3u", p.nodes);
        d.setTextColor(p.rssi >= -70 ? kFieldGreen : kCalibrationYellow, bg);
        d.printf(" %4d", p.rssi);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" L%u", p.lqi);
    }
    if (pans.empty()) {
        d.setCursor(4, kListTop);
        d.setTextColor(kDimGreen, kVoidInk);
        d.print(m.active() ? "LISTENING FOR PAN/NODE FRAMES..." : "NO 802.15.4 OBSERVATIONS YET");
    }
}

}  // namespace

void drawZigView(const model::ZigModel &m, ZigTab tab, size_t cursor, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const bool pans_tab = tab == ZigTab::Pans;
    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(pans_tab ? kPaperPhosphor : kMutedSlate, kVoidInk);
    d.printf("PANS %u", (unsigned)m.pans().size());
    d.setCursor(kZigNodesX, kBodyTop + 2);
    d.setTextColor(pans_tab ? kMutedSlate : kPaperPhosphor, kVoidInk);
    d.printf("NODES %u", (unsigned)m.nodes().size());
    d.setCursor(kZigStatusX, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print(m.active() ? "LISTENING" : "IDLE");
    if (m.malformedRows()) {
        d.setTextColor(kSignalPink, kVoidInk);
        d.printf(" BAD %u", m.malformedRows());
    }

    if (pans_tab) drawPanRows(m, cursor);
    else drawNodeRows(m, cursor);

    d.setCursor(4, kDetailRowY);
    if (pans_tab) {
        const auto &pans = m.pans();
        if (!pans.empty() && cursor < pans.size()) {
            const auto &p = pans[cursor];
            d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
            d.printf("%s · %d CH · N%u · RSSI %d", printable(p.pan, 8).c_str(),
                     channelCount(p.channels), p.nodes, p.rssi);
        } else {
            d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
            d.printf("PANS %u · %s", (unsigned)pans.size(), m.active() ? "LISTENING" : "IDLE");
        }
    } else {
        const auto &nodes = m.nodes();
        if (!nodes.empty() && cursor < nodes.size()) {
            const auto &n = nodes[cursor];
            d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
            if (n.short_addr.empty() && !n.ext.empty()) {
                d.printf("EXT %s · RSSI %d LQI %u", printable(n.ext, 16).c_str(), n.rssi, n.lqi);
            } else {
                d.printf("%s RSSI %d LQI %u", printable(n.pan, 8).c_str(), n.rssi, n.lqi);
            }
        } else {
            d.setTextColor(m.active() ? kFieldGreen : kMutedSlate, kVoidInk);
            d.printf("NODES %u · %s", (unsigned)nodes.size(), m.active() ? "LISTENING" : "IDLE");
        }
    }

    endChrome(chrome);
}
}  // namespace ui
