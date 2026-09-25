/*
 * oled.cpp — see oled.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "oled.h"

#include <Arduino.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>

#include <atomic>

namespace {

constexpr uint32_t kOledI2cHz = 400000;
constexpr TickType_t kRefresh = pdMS_TO_TICKS(250);
constexpr BaseType_t kCore = 0;   /* sampling loop owns core 1 */

/* Same address and look; different controllers. Chosen by #panel, kept in NVS. */
U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C g_ssd1306(U8G2_R0, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C g_sh1106(U8G2_R0, U8X8_PIN_NONE);

uint8_t g_addr = 0;
int g_sda = -1, g_scl = -1;
std::atomic<int> g_panel{0};          /* OledPanel */
std::atomic<int> g_panel_wanted{0};
std::atomic<uint32_t> g_frames{0};
portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
meter::DisplaySnapshot g_snap;

U8G2 &panel(int p)
{
    if (p == static_cast<int>(OledPanel::Sh1106)) return g_sh1106;
    return g_ssd1306;
}

bool answers(uint8_t addr)
{
    Wire1.beginTransmission(addr);
    return Wire1.endTransmission() == 0;
}

bool start(U8G2 &u)
{
    u.setI2CAddress(g_addr * 2);
    u.setBusClock(kOledI2cHz);
    return u.begin();
}

void draw(U8G2 &u, const meter::DisplayLines &l)
{
    u.clearBuffer();
    u.setFont(u8g2_font_6x10_tr);
    u.drawStr(0, 9, l.head);
    u.drawHLine(0, 11, 128);
    u.setFont(u8g2_font_logisoso16_tr);
    u.drawStr(0, 31, l.big);
    u.setFont(u8g2_font_6x10_tr);
    u.drawStr(0, 43, l.range);
    u.drawStr(0, 53, l.flags);
    u.drawStr(0, 63, l.label);
    u.sendBuffer();
}

void task(void *)
{
    meter::DisplayLines lines;
    for (;;) {
        int want = g_panel_wanted.load();
        if (want != g_panel.load()) {
            start(panel(want));
            g_panel = want;
        }
        meter::DisplaySnapshot s;
        portENTER_CRITICAL(&g_lock);
        s = g_snap;
        portEXIT_CRITICAL(&g_lock);
        meter::composeLines(s, lines);
        draw(panel(g_panel.load()), lines);
        g_frames++;
        vTaskDelay(kRefresh);
    }
}

void scanInto(TwoWire &w, char *out, size_t cap)
{
    size_t len = 0;
    out[0] = '\0';
    for (uint8_t a = 0x08; a < 0x78; a++) {
        w.beginTransmission(a);
        if (w.endTransmission() != 0) continue;
        int n = snprintf(out + len, cap - len, "%s0x%02X", len ? "," : "", a);
        if (n < 0 || static_cast<size_t>(n) >= cap - len) break;
        len += static_cast<size_t>(n);
    }
    if (!len) snprintf(out, cap, "none");
}

}  // namespace

void oledBegin(int sda, int scl)
{
    g_sda = sda;
    g_scl = scl;
    Preferences prefs;
    if (prefs.begin("meter", true)) {
        int p = prefs.getUChar("panel", 0) == 1 ? 1 : 0;
        g_panel = p;
        g_panel_wanted = p;
        prefs.end();
    }
    Wire1.begin(sda, scl, kOledI2cHz);
    Wire1.setTimeOut(20);
}

bool oledAttach()
{
    if (g_addr) return true;
    for (uint8_t a : {uint8_t{0x3C}, uint8_t{0x3D}}) {
        if (answers(a)) {
            g_addr = a;
            break;
        }
    }
    if (!g_addr) return false;
    if (!start(panel(g_panel.load()))) {
        g_addr = 0;
        return false;
    }
    return xTaskCreatePinnedToCore(task, "oled", 4096, nullptr, 1, nullptr, kCore) == pdPASS;
}

void oledSetPanel(OledPanel p)
{
    int v = static_cast<int>(p);
    Preferences prefs;
    if (prefs.begin("meter", false)) {
        prefs.putUChar("panel", static_cast<uint8_t>(v));
        prefs.end();
    }
    g_panel_wanted = v;                 /* the task switches between frames */
    if (!g_addr) g_panel = v;
}

const char *oledPanelName()
{
    return g_panel.load() == static_cast<int>(OledPanel::Sh1106) ? "sh1106" : "ssd1306";
}

uint32_t oledFrames()
{
    return g_frames.load();
}

void oledScan(char *as_wired, char *swapped, size_t cap)
{
    if (g_addr) {   /* panel in use: its task owns the bus */
        snprintf(as_wired, cap, "0x%02X(in_use)", g_addr);
        snprintf(swapped, cap, "skipped");
        return;
    }
    scanInto(Wire1, as_wired, cap);
    Wire1.end();
    Wire1.begin(g_scl, g_sda, kOledI2cHz);
    scanInto(Wire1, swapped, cap);
    Wire1.end();
    Wire1.begin(g_sda, g_scl, kOledI2cHz);
    Wire1.setTimeOut(20);
}

uint8_t oledAddress()
{
    return g_addr;
}

void oledPublish(const meter::DisplaySnapshot &s)
{
    portENTER_CRITICAL(&g_lock);
    g_snap = s;
    portEXIT_CRITICAL(&g_lock);
}
