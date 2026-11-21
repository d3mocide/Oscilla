/*
 * observatory_chrome.h — Observatory v2 shared external 320x240 primitives.
 *
 * Implements the chamfered aperture, truth-state pill, #151018 reading dock,
 * and 1-bit Chakra Petch / IBM Plex Mono typography from docs/brand/README.md.
 * Solid RGB565 only; zero alpha, gradients, or heap allocation.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include "ui/external_instrument.h"
#include "ui/observatory_fonts.h"

namespace ui {
namespace observatory {

using instrument::Canvas;
using instrument::safe;

enum class ModeSigil : uint8_t {
    HexOrbit, WaveScope, PulseBars, VectorTrail, FixReticle,
    NodeMesh, ShieldAlert, SystemCore, ArchiveDisk
};
enum class BadgeTone : uint8_t { Live, Warn, Cached, Fault };
enum class Align : uint8_t { Left, Center, Right };

inline std::string obsUpper(std::string s) {
    for (char &ch : s) if (ch >= 'a' && ch <= 'z') ch = char(ch - 32);
    return s;
}

inline char cleanChar(unsigned char ch) {
    return (ch >= 0x20 && ch <= 0x7E) ? char(ch) : '.';
}

inline int textWidth(const std::string &s, FontId id) {
    const Font &f = font(id);
    int w = 0;
    for (unsigned char raw : s) {
        const uint8_t ch = static_cast<uint8_t>(cleanChar(raw));
        w += f.glyphs[ch - f.first_char].x_advance;
    }
    return w;
}

inline std::string fitText(const std::string &raw, FontId id, int max_w) {
    std::string s = safe(raw, 64);
    if (max_w <= 0 || textWidth(s, id) <= max_w) return s;
    const int dots = textWidth("...", id);
    if (max_w <= dots) return "";
    while (!s.empty() && textWidth(s, id) + dots > max_w) s.pop_back();
    return s + "...";
}

inline int obsText(Canvas &c, int x, int y, const std::string &raw, FontId id,
                   uint16_t color, int max_w = 0, Align align = Align::Left) {
    const Font &f = font(id);
    const std::string s = fitText(raw, id, max_w);
    const int w = textWidth(s, id);
    int cx = align == Align::Right ? x - w : align == Align::Center ? x - w / 2 : x;
    if (y - 4 >= c.top() + c.stripHeight() || y + int(f.line_height) + 4 <= c.top()) return w;
    for (unsigned char ch_raw : s) {
        const uint8_t ch = static_cast<uint8_t>(cleanChar(ch_raw));
        const Glyph &g = f.glyphs[ch - f.first_char];
        if (g.width && g.height) {
            const int gx = cx + g.x_offset;
            const int gy = y + g.y_offset;
            const int stride = (g.width + 7) / 8;
            const int r0 = std::max(0, c.top() - gy);
            const int r1 = std::min<int>(g.height, c.top() + c.stripHeight() - gy);
            for (int r = r0; r < r1; ++r) {
                const uint8_t *row_ptr = f.bitmap + g.offset + r * stride;
                const int py = gy + r;
                for (int col = 0; col < g.width; ++col) {
                    if (row_ptr[col >> 3] & (0x80u >> (col & 7))) {
                        c.pixel(gx + col, py, color);
                    }
                }
            }
        }
        cx += g.x_advance;
    }
    return w;
}

inline void obsMicro(Canvas &c, int x, int y, const std::string &s, uint16_t color,
                     uint16_t bg = kVoidInk, Align align = Align::Left) {
    const std::string clean = safe(s, 53);
    const int w = int(clean.size()) * 6;
    const int dx = align == Align::Right ? x - w : align == Align::Center ? x - w / 2 : x;
    c.text(std::max(0, dx), y, clean, color, 1, bg);
}

inline void obsDashedH(Canvas &c, int x0, int x1, int y, uint16_t color, int on = 2, int off = 3) {
    if (y < c.top() || y >= c.top() + c.stripHeight()) return;
    const int step = on + off;
    for (int x = x0; x <= x1; x += step) {
        c.line(x, y, std::min(x1, x + on - 1), y, color);
    }
}

inline void obsDashedV(Canvas &c, int x, int y0, int y1, uint16_t color, int on = 2, int off = 3) {
    const int step = on + off;
    for (int y = y0; y <= y1; y += step) {
        c.line(x, y, x, std::min(y1, y + on - 1), color);
    }
}

inline void obsDashedCircle(Canvas &c, int cx, int cy, int r, uint16_t color, int segs = 36) {
    for (int i = 0; i < segs; i += 2) {
        const double a0 = i * 6.283185307179586 / segs;
        const double a1 = (i + 1) * 6.283185307179586 / segs;
        c.line(cx + int(std::lround(std::cos(a0) * r)), cy + int(std::lround(std::sin(a0) * r)),
               cx + int(std::lround(std::cos(a1) * r)), cy + int(std::lround(std::sin(a1) * r)), color);
    }
}

inline void obsHexagon(Canvas &c, int cx, int cy, int r, uint16_t color) {
    int vx[6], vy[6];
    for (int i = 0; i < 6; ++i) {
        const double a = (i * 60 - 90) * 0.0174532925199433;
        vx[i] = cx + int(std::lround(std::cos(a) * r));
        vy[i] = cy + int(std::lround(std::sin(a) * r));
    }
    for (int i = 0; i < 6; ++i) c.line(vx[i], vy[i], vx[(i + 1) % 6], vy[(i + 1) % 6], color);
}

inline void obsDiamond(Canvas &c, int x, int y, int r, uint16_t color) {
    c.line(x, y - r, x + r, y, color);
    c.line(x + r, y, x, y + r, color);
    c.line(x, y + r, x - r, y, color);
    c.line(x - r, y, x, y - r, color);
}

inline void obsDiamondFill(Canvas &c, int x, int y, int r, uint16_t color) {
    for (int dy = -r; dy <= r; ++dy) {
        const int span = r - std::abs(dy);
        c.line(x - span, y + dy, x + span, y + dy, color);
    }
}

inline void obsTargetLock(Canvas &c, int x, int y, int half, uint16_t color) {
    const int arm = 4;
    c.line(x - half, y - half, x - half + arm, y - half, color);
    c.line(x - half, y - half, x - half, y - half + arm, color);
    c.line(x + half, y - half, x + half - arm, y - half, color);
    c.line(x + half, y - half, x + half, y - half + arm, color);
    c.line(x - half, y + half, x - half + arm, y + half, color);
    c.line(x - half, y + half, x - half, y + half - arm, color);
    c.line(x + half, y + half, x + half - arm, y + half, color);
    c.line(x + half, y + half, x + half, y + half - arm, color);
}

inline void obsSegBar(Canvas &c, int x, int y, int w, int h, int segs, int pct, uint16_t color) {
    const int clamped = std::max(0, std::min(100, pct));
    const int active = (clamped * segs + 50) / 100;
    const int gap = 2;
    const int step = (w + gap) / segs;
    const int sw = std::max(2, step - gap);
    for (int i = 0; i < segs; ++i) {
        c.rect(x + i * step, y, sw, h, i < active ? color : kTrackDark);
    }
}

inline void obsGlyph(Canvas &c, int x, int y, ModeSigil g) {
    switch (g) {
        case ModeSigil::HexOrbit:
            obsHexagon(c, x + 4, y + 5, 5, kFieldGreen);
            c.dot(x + 4, y + 5, 1, kCalibrationYellow);
            break;
        case ModeSigil::WaveScope:
            c.line(x, y + 7, x + 2, y + 7, kFieldGreen);
            c.line(x + 2, y + 7, x + 4, y + 2, kFieldGreen);
            c.line(x + 4, y + 2, x + 6, y + 8, kFieldGreen);
            c.line(x + 6, y + 8, x + 8, y + 5, kFieldGreen);
            c.line(x + 8, y + 5, x + 10, y + 5, kFieldGreen);
            break;
        case ModeSigil::PulseBars:
            c.rect(x + 1, y + 5, 2, 5, kDimGreen);
            c.rect(x + 4, y + 1, 2, 9, kCalibrationYellow);
            c.rect(x + 7, y + 4, 2, 6, kFieldGreen);
            break;
        case ModeSigil::VectorTrail:
            c.line(x + 1, y + 8, x + 4, y + 3, kFieldGreen);
            c.line(x + 4, y + 3, x + 7, y + 6, kFieldGreen);
            c.line(x + 7, y + 6, x + 9, y + 2, kFieldGreen);
            c.dot(x + 9, y + 2, 1, kSignalPink);
            break;
        case ModeSigil::FixReticle:
            c.ring(x + 4, y + 5, 4, kFieldGreen);
            c.line(x + 4, y, x + 4, y + 10, kDimGreen);
            c.line(x - 1, y + 5, x + 9, y + 5, kDimGreen);
            c.dot(x + 4, y + 5, 1, kCalibrationYellow);
            break;
        case ModeSigil::NodeMesh:
            c.line(x + 4, y + 5, x + 1, y + 2, kDimGreen);
            c.line(x + 4, y + 5, x + 8, y + 2, kDimGreen);
            c.line(x + 4, y + 5, x + 7, y + 8, kSignalPink);
            c.dot(x + 4, y + 5, 2, kPaperPhosphor);
            c.dot(x + 1, y + 2, 1, kFieldGreen);
            c.dot(x + 8, y + 2, 1, kFieldGreen);
            c.dot(x + 7, y + 8, 1, kCalibrationYellow);
            break;
        case ModeSigil::ShieldAlert:
            obsDiamond(c, x + 4, y + 5, 4, kSignalPink);
            c.line(x + 4, y + 3, x + 4, y + 5, kCalibrationYellow);
            c.pixel(x + 4, y + 7, kCalibrationYellow);
            break;
        case ModeSigil::SystemCore:
            obsHexagon(c, x + 4, y + 5, 5, kFieldGreen);
            obsDiamond(c, x + 4, y + 5, 2, kPaperPhosphor);
            c.pixel(x + 4, y + 5, kCalibrationYellow);
            break;
        case ModeSigil::ArchiveDisk:
            c.rect(x + 1, y + 1, 8, 2, kFieldGreen);
            c.rect(x + 1, y + 4, 8, 2, kDimGreen);
            c.rect(x + 1, y + 7, 8, 2, kCalibrationYellow);
            break;
    }
}

inline void obsFrame(Canvas &c, ModeSigil glyph, const char *title, const char *badge, BadgeTone tone) {
    c.clear();
    obsGlyph(c, 10, 7, glyph);
    obsText(c, 25, 7, title, FontId::Head, kPaperPhosphor, 212);

    const uint16_t fill = tone == BadgeTone::Live ? kTrackDark
                        : tone == BadgeTone::Warn ? kSelectionGlow : kPanelDark;
    const uint16_t stroke = tone == BadgeTone::Live ? kFieldGreen
                          : tone == BadgeTone::Warn ? kCalibrationYellow
                          : tone == BadgeTone::Fault ? kFaultRed : kLineBorder;
    const uint16_t fg = tone == BadgeTone::Live ? kFieldGreen
                      : tone == BadgeTone::Warn ? kCalibrationYellow
                      : tone == BadgeTone::Fault ? kFaultRed : kMutedSlate;
    const int bw = std::max(46, int(std::strlen(badge)) * 6 + 18);
    const int bx = 312 - bw;
    c.rect(bx + 1, 6, bw - 2, 12, fill);
    c.line(bx + 4, 5, 308, 5, stroke);
    c.line(308, 5, 312, 9, stroke);
    c.line(312, 9, 312, 18, stroke);
    c.line(bx, 18, 312, 18, stroke);
    c.line(bx, 9, bx, 18, stroke);
    c.line(bx, 9, bx + 4, 5, stroke);
    if (tone == BadgeTone::Live) c.rect(bx + 6, 10, 3, 3, fg);
    else c.box(bx + 6, 10, 3, 3, fg);
    c.text(bx + 12, 8, badge, fg, 1, fill);

    /* Chamfered octagonal aperture: static for SPI tile-diffing. */
    c.line(14, 25, 306, 25, kDimGreen);
    c.line(306, 25, 312, 31, kDimGreen);
    c.line(312, 31, 312, 145, kDimGreen);
    c.line(312, 145, 306, 151, kDimGreen);
    c.line(306, 151, 14, 151, kDimGreen);
    c.line(14, 151, 8, 145, kDimGreen);
    c.line(8, 145, 8, 31, kDimGreen);
    c.line(8, 31, 14, 25, kDimGreen);
    /* Bright chamfer corner accents + top/bottom center datum ticks. */
    c.line(8, 41, 8, 31, kFieldGreen); c.line(8, 31, 14, 25, kFieldGreen); c.line(14, 25, 24, 25, kFieldGreen);
    c.line(296, 25, 306, 25, kFieldGreen); c.line(306, 25, 312, 31, kFieldGreen); c.line(312, 31, 312, 41, kFieldGreen);
    c.line(8, 135, 8, 145, kFieldGreen); c.line(8, 145, 14, 151, kFieldGreen); c.line(14, 151, 24, 151, kFieldGreen);
    c.line(296, 151, 306, 151, kFieldGreen); c.line(306, 151, 312, 145, kFieldGreen); c.line(312, 145, 312, 135, kFieldGreen);
    c.rect(154, 25, 12, 2, kFieldGreen);
    c.rect(154, 150, 12, 2, kFieldGreen);
}

inline void obsDock(Canvas &c, uint16_t accent, bool three_bays = true) {
    c.rect(0, 156, 320, 84, kPanelDark);
    c.line(0, 156, 319, 156, kDimGreen);
    c.rect(8, 155, 42, 2, accent);
    c.line(12, 198, 308, 198, kLineBorder);
    if (three_bays) {
        c.line(112, 202, 112, 235, kLineBorder);
        c.line(214, 202, 214, 235, kLineBorder);
    }
}

inline void obsCard(Canvas &c, int x, int y, int w, int h, uint16_t accent) {
    c.rect(x + 1, y + 1, w - 2, h - 2, kPanelDark);
    c.line(x, y, x + w - 5, y, kLineBorder);
    c.line(x + w - 5, y, x + w - 1, y + 4, kLineBorder);
    c.line(x + w - 1, y + 4, x + w - 1, y + h - 1, kLineBorder);
    c.line(x, y + h - 1, x + w - 1, y + h - 1, kLineBorder);
    c.line(x, y, x, y + h - 1, kLineBorder);
    c.rect(x, y, 32, 2, accent);
}

inline void obsBadge(Canvas &c, int x, int y, int w, int h, const std::string &text,
                     uint16_t fill, uint16_t stroke, uint16_t fg) {
    c.rect(x, y, w, h, fill);
    c.box(x, y, w, h, stroke);
    obsMicro(c, x + w / 2, y + (h - 7) / 2, text, fg, fill, Align::Center);
}

inline void obsBay(Canvas &c, int x, int max_w, const char *label, const std::string &value,
                   uint16_t color, const char *unit = "", uint16_t unit_color = kMutedSlate) {
    obsMicro(c, x, 203, label, kMutedSlate, kPanelDark);
    const int unit_w = (unit && *unit) ? int(std::strlen(unit)) * 6 + 4 : 0;
    FontId id = FontId::Hero;
    if (textWidth(value, id) + unit_w > max_w) id = FontId::Title;
    if (textWidth(value, id) + unit_w > max_w) id = FontId::Mono;
    const int ty = id == FontId::Hero ? 216 : id == FontId::Title ? 218 : 221;
    const int vw = obsText(c, x, ty, value, id, color, std::max(12, max_w - unit_w));
    if (unit && *unit && x + vw + 4 < x + max_w) {
        obsMicro(c, x + vw + 4, 224, unit, unit_color, kPanelDark);
    }
}

}  // namespace observatory
}  // namespace ui
