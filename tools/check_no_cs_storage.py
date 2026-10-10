#!/usr/bin/env python3
"""Compile the real storage module against counters; no hardware required."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware-cardputer/src/storage/sd_storage.cpp"


def check(source, directory, isolated, coexist=False, integrated=False):
    executable = directory / "storage-test"
    command = ["g++", "-std=c++17", "-I" + str(directory),
               "-I" + str(ROOT / "firmware-cardputer/src"),
               str(source), str(directory / "test.cpp"), "-o", str(executable)]
    if isolated:
        command.append("-DOSCILLA_NO_CS_DISPLAY_BENCH")
    if coexist:
        command.append("-DOSCILLA_SD_DISPLAY_BENCH")
    if integrated:
        command.append("-DOSCILLA_EXTERNAL_NO_CS_DISPLAY")
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
#ifdef OSCILLA_NO_CS_DISPLAY_BENCH
    return started || ready || recovered || mounts || unmounts;
#elif defined(OSCILLA_SD_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
    return !(started && ready && recovered && mounts == 2 && unmounts == 1 && fastest <= 10000000);
#else
    return !(started && ready && recovered && mounts == 2 && unmounts == 1);
#endif
}
""")
        assert check(SOURCE, directory, False) == 0, "normal SD path regressed"
        assert check(SOURCE, directory, True) == 0, "no-CS bench reached SD"
        assert check(SOURCE, directory, False, True) == 0, "coexistence clock exceeds panel timing"
        assert check(SOURCE, directory, False, integrated=True) == 0, "integrated display clock exceeds timing"
        mutated = directory / "mutated.cpp"
        text = SOURCE.read_text()
        assert text.count("#ifdef OSCILLA_NO_CS_DISPLAY_BENCH") == 2
        mutated.write_text(text.replace("#ifdef OSCILLA_NO_CS_DISPLAY_BENCH", "#if 0"))
        assert check(mutated, directory, True) != 0, "missing guard went undetected"
        mutated.write_text(text.replace("kSpiHz = 8000000", "kSpiHz = 25000000"))
        assert check(mutated, directory, False, True) != 0, "panel overclock went undetected"
        assert check(mutated, directory, False, integrated=True) != 0, "integrated panel overclock went undetected"
    print("no-CS storage: mount/remount, isolation, panel clock cap and rejected mutations passed")


if __name__ == "__main__":
    main()
