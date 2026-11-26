/* Native ST7789 path matching the working colour fixture; Rev D §5.4. */
#include "ui/external_tft.h"
#include <Arduino.h>
#include <algorithm>
#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#include "storage/sd_storage.h"

namespace ui {
namespace {
constexpr int kCs = 5, kReset = 3;
#ifndef OSCILLA_EXTERNAL_TFT_HZ
#define OSCILLA_EXTERNAL_TFT_HZ 8000000
#endif
class ExternalTft : public lgfx::LGFX_Device {
    lgfx::Bus_SPI bus_;
    lgfx::Panel_ST7789 panel_;
public:
    ExternalTft()
    {
        auto bus = bus_.config();
        bus.spi_host = SPI2_HOST; /* Same controller as Arduino FSPI/SD on S3. */
        bus.spi_mode = 0;
        bus.freq_write = bus.freq_read = OSCILLA_EXTERNAL_TFT_HZ;
        bus.pin_sclk = 40;
        bus.pin_mosi = 14;
        bus.pin_miso = 39; /* Keep the SD input routed when the driver initializes. */
        bus.pin_dc = 6;
        bus.spi_3wire = false;
        bus.use_lock = true;
        bus.dma_channel = 0;
        bus_.config(bus);
        panel_.setBus(&bus_);
        auto panel = panel_.config();
        panel.pin_cs = kCs;
        panel.pin_rst = kReset;
        panel.memory_width = panel.panel_width = 240;
        panel.memory_height = panel.panel_height = 320;
        panel.readable = false;
        panel.invert = false;
        panel.bus_shared = true;
        panel_.config(panel);
        setPanel(&panel_);
    }
};
ExternalTft display;
ExternalTftStatus status;
}

void holdExternalTftReset()
{
    digitalWrite(kCs, HIGH);
    pinMode(kCs, OUTPUT);
    digitalWrite(kReset, LOW);
    pinMode(kReset, OUTPUT);
}

bool beginExternalTft()
{
    status = {};
    status.clock_hz = OSCILLA_EXTERNAL_TFT_HZ;
    if (!storage::lock(20)) return false;
    status.initialized = display.init();
    if (status.initialized) {
        display.setColorDepth(16);
        display.setRotation(1);
        display.setSwapBytes(true);
    }
    storage::unlock();
    return status.initialized;
}

bool setExternalTftSleep(bool sleep)
{
    if (!status.initialized) return false;
    if (status.sleeping == sleep) return true;
    if (!storage::lock(20)) return false;
    if (sleep) display.sleep();
    else display.wakeup();
    status.sleeping = sleep;
    storage::unlock();
    return true;
}

bool writeExternalTftSpan(int y,int x,int width,const uint16_t *pixels)
{
    if (!status.initialized || status.sleeping || !pixels || y < 0 || y >= 240 || x<0 || width<=0 || width>320 || x>320-width) return false;
    if (!storage::lock(2)) return false;
    const uint32_t started=micros();
    display.pushImage(x, y, width, 1, pixels);
    status.max_span_us=std::max(status.max_span_us,uint32_t(micros()-started));
    ++status.rows_sent;
    status.pixels_sent+=width;
    storage::unlock();
    return true;
}

const ExternalTftStatus &externalTftStatus() { return status; }
}
