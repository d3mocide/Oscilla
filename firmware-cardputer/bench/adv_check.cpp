/*
 * adv_check.cpp — D-12 hardware check, not the deck app.
 *
 * Proves on the delivered Cardputer ADV that M5Unified detects the board,
 * drives the display, reads the TCA8418 keyboard, and leaves the Grove UART
 * (GPIO1/2) usable: it runs the same USB <-> Grove relay as grove_bridge.cpp
 * so `ocp_repl.py --gate` can be run through it with M5Unified active.
 *
 * Diagnostics go to USB as `d12 ...` lines, which the OCP parser treats as
 * noise.
 *
 * SPDX-License-Identifier: MIT
 */

#include <M5Cardputer.h>

#include "ocp.h"

static constexpr int kGroveTxPin = 2;   /* Rev D §3 */
static constexpr int kGroveRxPin = 1;
static constexpr size_t kBufBytes = 1024;

static size_t s_up_bytes;     /* host -> probe */
static size_t s_down_bytes;   /* probe -> host */
static String s_typed;
static bool   s_dirty = true;

static bool relay(Stream &from, Stream &to, uint8_t *buf, size_t &count)
{
    int avail = from.available();
    if (avail <= 0) return false;
    size_t n = from.readBytes(buf, min((size_t)avail, kBufBytes));
    to.write(buf, n);
    count += n;
    return true;
}

static void report_boot(void)
{
    auto board = M5.getBoard();
    Serial.printf("d12 board=%d adv=%d\n", (int)board,
                  board == m5::board_t::board_M5CardputerADV);
    Serial.printf("d12 display=%dx%d\n", M5Cardputer.Display.width(),
                  M5Cardputer.Display.height());
    /* 1 means Port A (GPIO1/2) was assigned, not started; see cardputer-adv.md. */
    Serial.printf("d12 ex_i2c_assigned=%d\n", M5.Ex_I2C.isEnabled());

    bool found[128] = {};
    M5.In_I2C.scanID(found);
    Serial.print("d12 in_i2c:");
    for (int a = 8; a < 120; a++) {
        if (found[a]) Serial.printf(" 0x%02x", a);
    }
    Serial.println();
}

static void draw(void)
{
    auto &d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.println("OSCILLA  D-12 check");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.printf("board %d  %s\n", (int)M5.getBoard(),
             M5.getBoard() == m5::board_t::board_M5CardputerADV ? "ADV" : "NOT ADV");
    d.printf("lcd %dx%d\n", d.width(), d.height());
    d.printf("grove up %u  down %u\n", (unsigned)s_up_bytes, (unsigned)s_down_bytes);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.println("type to test keys:");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.println(s_typed);

    /* Colour bars: a wrong panel init shows as wrong or missing colours. */
    const uint16_t bars[] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE };
    int w = d.width() / 4, y = d.height() - 16;
    for (int i = 0; i < 4; i++) d.fillRect(i * w, y, w, 16, bars[i]);
}

static void poll_keys(void)
{
    M5Cardputer.update();
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;

    auto st = M5Cardputer.Keyboard.keysState();
    for (char c : st.word) s_typed += c;
    if (st.del && s_typed.length()) s_typed.remove(s_typed.length() - 1);
    if (st.enter) s_typed = "";
    if (s_typed.length() > 38) s_typed.remove(0, s_typed.length() - 38);

    Serial.printf("d12 key word=\"");
    for (char c : st.word) Serial.print(c);
    Serial.printf("\" fn=%d shift=%d ctrl=%d opt=%d alt=%d del=%d enter=%d tab=%d space=%d\n",
                  st.fn, st.shift, st.ctrl, st.opt, st.alt, st.del, st.enter, st.tab, st.space);
    s_dirty = true;
}

void setup()
{
    Serial.setRxBufferSize(kBufBytes);
    Serial.setTxBufferSize(kBufBytes);
    Serial.begin(OCP_BAUD_DEFAULT);
    Serial.setTxTimeoutMs(0);

    auto cfg = M5.config();   /* defaults: external_rtc/imu off, Port A untouched */
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);

    Serial1.setRxBufferSize(kBufBytes);
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);

    delay(1500);   /* let a host open the port before the one-shot report */
    report_boot();
}

void loop()
{
    static uint8_t buf[kBufBytes];
    static uint32_t last_draw_ms;
    bool moved = relay(Serial1, Serial, buf, s_down_bytes);
    moved |= relay(Serial, Serial1, buf, s_up_bytes);

    poll_keys();

    uint32_t now = millis();
    if ((s_dirty || moved) && now - last_draw_ms > 250) {
        draw();
        s_dirty = false;
        last_draw_ms = now;
    }
    if (!moved) delay(1);
}
