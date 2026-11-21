#!/usr/bin/env python3
"""Check the ST7789 deck's SD mount/remount and conservative clock cap."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware-cardputer/src/storage/sd_storage.cpp"


def check(source, directory):
    executable = directory / "storage-test"
    command = ["g++", "-std=c++17", "-I" + str(directory),
               "-I" + str(ROOT / "firmware-cardputer/src"),
               str(source), str(directory / "test.cpp"), "-o", str(executable)]
    subprocess.run(command, check=True, capture_output=True)
    return subprocess.run([str(executable)], capture_output=True).returncode


def main():
    with tempfile.TemporaryDirectory(prefix="oscilla-storage-") as name:
        directory = pathlib.Path(name)
        (directory / "freertos").mkdir()
        (directory / "freertos/FreeRTOS.h").write_text(
            "#pragma once\n#include <cstdint>\n#define pdTRUE 1\n#define pdMS_TO_TICKS(x) (x)\n")
        (directory / "freertos/semphr.h").write_text("""#pragma once
using SemaphoreHandle_t = void*;
inline void* xSemaphoreCreateMutex() { return reinterpret_cast<void*>(1); }
inline int xSemaphoreTake(void*, unsigned) { return 1; }
inline void xSemaphoreGive(void*) {}
""")
        (directory / "SPI.h").write_text("""#pragma once
struct Spi { void begin(int, int, int, int) {} };
extern Spi SPI;
""")
        (directory / "SD.h").write_text("""#pragma once
#include <SPI.h>
extern unsigned mounts, unmounts, fastest;
struct Card {
    bool begin(int, Spi&, unsigned hz) {
        ++mounts;
        if (hz > fastest) fastest = hz;
        return true;
    }
    void end() { ++unmounts; }
};
extern Card SD;
""")
        (directory / "test.cpp").write_text("""#include <SD.h>
#include "storage/sd_storage.h"
Spi SPI; Card SD; unsigned mounts, unmounts, fastest;
int main() {
    bool started = storage::begin();
    bool ready = storage::ready();
    bool recovered = storage::remount();
    return !(started && ready && recovered && mounts == 2 && unmounts == 1 && fastest == 8000000);
}
""")
        assert check(SOURCE, directory) == 0, "SD mount/remount or 8 MHz cap regressed"
        mutated = directory / "mutated.cpp"
        text = SOURCE.read_text()
        assert "kSpiHz = 8000000" in text
        mutated.write_text(text.replace("kSpiHz = 8000000", "kSpiHz = 25000000"))
        assert check(mutated, directory) != 0, "SD overclock went undetected"
    print("SD storage: mount/remount, 8 MHz cap and rejected overclock mutation passed")


if __name__ == "__main__":
    main()
