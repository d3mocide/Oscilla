#!/usr/bin/env python3
"""Check native TFT lock ownership, shared bus pins, failures and row bounds."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware-cardputer/src/ui/external_tft.cpp"


def main():
    with tempfile.TemporaryDirectory(prefix="oscilla-tft-") as name:
        directory = Path(name)
        (directory / "storage").mkdir()
        (directory / "lgfx/v1/panel").mkdir(parents=True)
        (directory / "Arduino.h").write_text("""#pragma once
#include <cstdint>
constexpr int HIGH=1, LOW=0, OUTPUT=1, SPI2_HOST=1;
void digitalWrite(int,int);
void pinMode(int,int);
uint32_t micros();
""")
        (directory / "storage/sd_storage.h").write_text("""#pragma once
namespace storage { bool lock(unsigned); void unlock(); }
""")
        (directory / "M5GFX.h").write_text("""#pragma once
#include <cstdint>
#include <cassert>
extern bool held, init_ok;
extern unsigned initializations, images, sleeps, wakeups;
namespace lgfx {
struct Bus_SPI {
    struct Config { int spi_host=-1,spi_mode=-1,pin_sclk=-1,pin_mosi=-1,pin_miso=-1,pin_dc=-1;
        unsigned freq_write=0,freq_read=0,dma_channel=1; bool spi_3wire=true,use_lock=false; };
    Config config() { return {}; }
    void config(Config c) { assert(c.spi_host==1 && c.spi_mode==0 && c.pin_sclk==40 && c.pin_mosi==14 && c.pin_miso==39 && c.pin_dc==6 && c.freq_write==OSCILLA_EXTERNAL_TFT_HZ && c.dma_channel==0 && !c.spi_3wire && c.use_lock); }
};
struct Panel_ST7789 {
    struct Config { int pin_cs=-1,pin_rst=-1,memory_width=0,panel_width=0,memory_height=0,panel_height=0;
        bool readable=true,invert=false,bus_shared=false; };
    Config config() { return {}; }
    void config(Config c) { assert(c.pin_cs==5 && c.pin_rst==3 && c.memory_width==240 && c.panel_width==240 && c.memory_height==320 && c.panel_height==320 && !c.readable && !c.invert && c.bus_shared); }
    void setBus(Bus_SPI*) {}
};
struct LGFX_Device {
    void setPanel(Panel_ST7789*) {}
    bool init() { assert(held); ++initializations; return init_ok; }
    void setRotation(int r) { assert(held && r==1); }
    void setColorDepth(int d) { assert(held && d==16); }
    void setSwapBytes(bool b) { assert(held && b); }
    void sleep() { assert(held); ++sleeps; }
    void wakeup() { assert(held); ++wakeups; }
    void pushImage(int x,int y,int w,int h,const uint16_t* p) {
        assert(held && x>=0 && x+w<=320 && y>=0 && y<240 && w>0 && h==1 && p); ++images;
    }
}; }
""")
        (directory / "lgfx/v1/panel/Panel_ST7789.hpp").write_text('#include <M5GFX.h>\n')
        test = directory / "test.cpp"
        test.write_text("""#include <cassert>
#include "ui/external_tft.h"
bool held=false, init_ok=false, allow_lock=true;
unsigned initializations=0, images=0, sleeps=0, wakeups=0;
void digitalWrite(int,int) {}
void pinMode(int,int) {}
uint32_t micros() { static uint32_t n=0; return n+=100; }
namespace storage {
bool lock(unsigned) { assert(!held); return held=allow_lock; }
void unlock() { assert(held); held=false; }
}
int main() {
    uint16_t row[320]{};
    assert(!ui::writeExternalTftRow(0,row));
    assert(!ui::setExternalTftSleep(true) && sleeps==0);
    allow_lock=false;
    assert(!ui::beginExternalTft() && initializations==0 && !held);
    allow_lock=true;
    assert(!ui::beginExternalTft() && initializations==1 && !held);
    assert(!ui::writeExternalTftRow(0,row));
    init_ok=true;
    assert(ui::beginExternalTft() && !held);
    assert(!ui::writeExternalTftRow(-1,row));
    assert(!ui::writeExternalTftRow(240,row));
    assert(!ui::writeExternalTftRow(0,nullptr));
    assert(images==0);
    allow_lock=false;
    assert(!ui::writeExternalTftRow(0,row) && images==0);
    assert(!ui::setExternalTftSleep(true) && sleeps==0 && !ui::externalTftStatus().sleeping);
    allow_lock=true;
    assert(ui::writeExternalTftRow(239,row) && images==1 && !held);
    assert(ui::externalTftStatus().rows_sent==1);
    assert(!ui::writeExternalTftSpan(0,319,2,row));
    assert(!ui::writeExternalTftSpan(0,0,0,row));
    assert(!ui::writeExternalTftSpan(0,-1,1,row));
    assert(ui::writeExternalTftSpan(0,280,40,row));
    assert(ui::externalTftStatus().pixels_sent==360);
    assert(ui::externalTftStatus().max_span_us==100);
    assert(ui::setExternalTftSleep(true) && sleeps==1 && ui::externalTftStatus().sleeping && !held);
    assert(ui::setExternalTftSleep(true) && sleeps==1);
    assert(!ui::writeExternalTftRow(0,row) && images==2);
    assert(ui::setExternalTftSleep(false) && wakeups==1 && !ui::externalTftStatus().sleeping && !held);
    assert(ui::writeExternalTftRow(0,row) && images==3 && !held);
}
""")

        def run(source, hz=1000000):
            executable = directory / "test"
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-DOSCILLA_EXTERNAL_TFT_HZ=" + str(hz),
                            "-I" + str(directory), "-I" + str(ROOT / "firmware-cardputer/src"),
                            str(source), str(test), "-o", str(executable)], check=True)
            return subprocess.run([str(executable)], capture_output=True).returncode

        assert run(SOURCE) == 0
        assert run(SOURCE, 8000000) == 0
        mutated = directory / "mutated.cpp"
        text = SOURCE.read_text()
        assert "if (!storage::lock(2)) return false;" in text
        mutated.write_text(text.replace("if (!storage::lock(2)) return false;", ""))
        assert run(mutated) != 0, "unlocked TFT traffic went undetected"
        assert "if (sleep) display.sleep();" in text
        mutated.write_text(text.replace("if (!storage::lock(20)) return false;\n    if (sleep)", "if (sleep)"))
        assert run(mutated) != 0, "unlocked TFT sleep went undetected"
    print("external TFT: shared controller/pins, init failures, row bounds, lock failures and rejected lock-bypass mutation passed")


if __name__ == "__main__":
    main()
