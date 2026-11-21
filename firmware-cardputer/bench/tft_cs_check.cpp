/* Isolated GMT028-05/ST7789 test; SD removed. Pins: Rev D §5. */
#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

namespace {
constexpr int kSdCs = 12, kTftCs = 5;

class TestDisplay : public lgfx::LGFX_Device {
    lgfx::Bus_SPI bus_;
    lgfx::Panel_ST7789 panel_;
public:
    TestDisplay()
    {
        auto bus = bus_.config();
        bus.spi_host = SPI2_HOST;
        bus.spi_mode = 0;
        bus.freq_write = 1000000;
        bus.freq_read = 1000000;
        bus.pin_sclk = 40;
        bus.pin_mosi = 14;
        bus.pin_miso = -1;
        bus.pin_dc = 6;
        bus.spi_3wire = false;
        bus.dma_channel = 0;
        bus_.config(bus);
        panel_.setBus(&bus_);
        auto panel = panel_.config();
        panel.pin_cs = kTftCs;
        panel.pin_rst = 3;
        panel.memory_width = panel.panel_width = 240;
        panel.memory_height = panel.panel_height = 320;
        panel.readable = false;
        panel.invert = false;
        panel.bus_shared = false; /* Isolated fixture: no SD driver or other bus user. */
        panel_.config(panel);
        setPanel(&panel_);
    }
};

TestDisplay display;
uint32_t lastPattern;
unsigned pattern;
bool initialized;

void drawPattern()
{
    const uint16_t colours[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
    if (pattern == 0) {
        for (int i = 0; i < 4; ++i) display.fillRect(i * 80, 0, 80, 240, colours[i]);
    } else {
        display.fillScreen(colours[pattern - 1]);
    }
    display.drawRect(0, 0, 320, 240, TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println("OSCILLA / ST7789 TEST");
    display.printf("1 MHz / pattern %u", pattern);
    Serial.printf("tft bench: pattern=%u sent; visual confirmation required\n", pattern);
    pattern = (pattern + 1) % 5;
    lastPattern = millis();
}
}

void setup()
{
    Serial.begin(115200);
    digitalWrite(kTftCs, HIGH);
    pinMode(kTftCs, OUTPUT);
    digitalWrite(kSdCs, HIGH);
    pinMode(kSdCs, OUTPUT);
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(4, 8);
    M5Cardputer.Display.println("TFT BENCH / ST7789");
    M5Cardputer.Display.println("SD REMOVED / 1 MHz");
    initialized = display.init();
    Serial.printf("tft bench: ST7789 init=%u; CS=5 RST=3 DC=6 SCK=40 MOSI=14; no readback\n", initialized);
    M5Cardputer.Display.println(initialized ? "Patterns sent" : "Driver init failed");
    if (initialized) {
        display.setRotation(1);
        drawPattern();
    }
}

void loop()
{
    if (initialized && millis() - lastPattern >= 3000) drawPattern();
    delay(10);
}
