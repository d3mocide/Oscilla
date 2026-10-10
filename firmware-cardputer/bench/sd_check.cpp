/* Isolated SD write/read test. Disconnect external CLK/MOSI; Rev D §5.2. */
#include <M5Cardputer.h>
#include <SD.h>
#include "storage/sd_storage.h"

namespace {
bool finished;
char path[48];
uint8_t block[256];

void report(const char *text)
{
    M5Cardputer.Display.println(text);
    Serial.println(text);
}

uint8_t expected(unsigned offset)
{
    return static_cast<uint8_t>((offset * 37u + offset / 256u) ^ 0xA5u);
}

void test()
{
    report("Mounting SD...");
    if (!storage::begin()) {
        report("FAIL: SD mount");
        return;
    }
    if (!storage::lock()) {
        report("FAIL: SD lock");
        return;
    }
    snprintf(path, sizeof(path), "/oscilla_sd_test_%08lx.tmp", (unsigned long)millis());
    if (SD.exists(path)) {
        storage::unlock();
        report("FAIL: test file exists");
        return;
    }
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        storage::unlock();
        report("FAIL: create file");
        return;
    }
    bool ok = true;
    for (unsigned chunk = 0; chunk < 16 && ok; ++chunk) {
        for (unsigned i = 0; i < sizeof(block); ++i) block[i] = expected(chunk * sizeof(block) + i);
        ok = file.write(block, sizeof(block)) == sizeof(block);
        delay(1);
    }
    file.flush();
    file.close();
    file = SD.open(path, FILE_READ);
    ok = ok && file && file.size() == 4096;
    for (unsigned chunk = 0; chunk < 16 && ok; ++chunk) {
        ok = file.read(block, sizeof(block)) == sizeof(block);
        for (unsigned i = 0; i < sizeof(block) && ok; ++i) {
            ok = block[i] == expected(chunk * sizeof(block) + i);
        }
        delay(1);
    }
    file.close();
    bool removed = SD.remove(path);
    storage::unlock();
    report(ok ? "PASS: 4096 bytes verified" : "FAIL: write/read mismatch");
    report(removed ? "Test file removed" : "Cleanup failed; temp remains");
    report("No existing files changed");
}
}

void setup()
{
    Serial.begin(115200);
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setCursor(0, 0);
    report("OSCILLA SD TEST / waiting");
    report("POWER OFF before rewiring.");
    report("Disconnect external CLK + MOSI.");
    report("Insert SD, then power on.");
    report("Press ENTER to test SD.");
}

void loop()
{
    M5Cardputer.update();
    if (!finished && M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed() && M5Cardputer.Keyboard.keysState().enter) {
        finished = true;
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setCursor(0, 0);
        test();
    }
    delay(1);
}
