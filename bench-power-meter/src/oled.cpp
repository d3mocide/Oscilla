/*
 * oled.cpp — see oled.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "oled.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

namespace {

constexpr uint32_t kOledI2cHz = 400000;
constexpr TickType_t kRefresh = pdMS_TO_TICKS(250);
constexpr BaseType_t kCore = 0;   /* sampling loop owns core 1 */

U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C g_u8g2(U8G2_R0, U8X8_PIN_NONE);
uint8_t g_addr = 0;
portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
meter::DisplaySnapshot g_snap;

bool answers(uint8_t addr)
{
    Wire1.beginTransmission(addr);
    return Wire1.endTransmission() == 0;
}

void draw(const meter::DisplayLines &l)
{
    g_u8g2.clearBuffer();
    g_u8g2.setFont(u8g2_font_6x10_tr);
    g_u8g2.drawStr(0, 9, l.head);
    g_u8g2.drawHLine(0, 11, 128);
    g_u8g2.setFont(u8g2_font_logisoso16_tr);
    g_u8g2.drawStr(0, 31, l.big);
    g_u8g2.setFont(u8g2_font_6x10_tr);
    g_u8g2.drawStr(0, 43, l.range);
    g_u8g2.drawStr(0, 53, l.flags);
    g_u8g2.drawStr(0, 63, l.label);
    g_u8g2.sendBuffer();
}

void task(void *)
{
    meter::DisplayLines lines;
    for (;;) {
        meter::DisplaySnapshot s;
        portENTER_CRITICAL(&g_lock);
        s = g_snap;
        portEXIT_CRITICAL(&g_lock);
        meter::composeLines(s, lines);
        draw(lines);
        vTaskDelay(kRefresh);
    }
}

}  // namespace

void oledBegin(int sda, int scl)
{
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
    g_u8g2.setI2CAddress(g_addr * 2);
    g_u8g2.setBusClock(kOledI2cHz);
    if (!g_u8g2.begin()) {
        g_addr = 0;
        return false;
    }
    return xTaskCreatePinnedToCore(task, "oled", 4096, nullptr, 1, nullptr, kCore) == pdPASS;
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
