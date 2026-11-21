/* ILI9341 SPI clock and shared-SD qualification; Rev D §5.2–5.3. */
#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <cstring>
#include "storage/sd_storage.h"

namespace {
constexpr int kClock = 40;
constexpr int kMosi = 14;
constexpr int kMiso = 39;
constexpr int kDc = 6;
constexpr int kReset = 3;
constexpr int kSdCs = 12;
constexpr uint32_t kSdHz = 8000000;
constexpr uint16_t kWidth = 320;
constexpr uint16_t kHeight = 240;
constexpr unsigned kFramesPerRun = 32;
constexpr size_t kFileSize = 4096;
uint8_t row[kWidth * 2];
char line[32];
size_t line_size;
bool line_overlong;
uint32_t tft_hz = 4000000;
uint32_t frame_ms_total, frame_ms_max;
unsigned frames_done, sd_passes;
const char *failure = "";
bool running;

void command(uint8_t value, const uint8_t *data = nullptr, size_t size = 0)
{
    digitalWrite(kDc, LOW);
    SPI.transfer(value);
    digitalWrite(kDc, HIGH);
    while (size--) SPI.transfer(*data++);
}

void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    const uint8_t columns[] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
    const uint8_t rows[] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
    command(0x2A, columns, sizeof(columns));
    command(0x2B, rows, sizeof(rows));
    command(0x2C);
}

void parkWindow()
{
    /* The delivered panel has no CS; confine later SD traffic to one pixel. */
    setWindow(319, 239, 319, 239);
}

bool initPanel()
{
    pinMode(kSdCs, OUTPUT);
    digitalWrite(kSdCs, HIGH);
    pinMode(kDc, OUTPUT);
    pinMode(kReset, OUTPUT);
    digitalWrite(kReset, LOW);
    delay(20);
    digitalWrite(kReset, HIGH);
    delay(150);

    if (!storage::lock(1000)) return false;
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    command(0x01);
    delay(150);
    const uint8_t format[] = {0x55}, orientation[] = {0x28};
    command(0x3A, format, sizeof(format));
    command(0x36, orientation, sizeof(orientation));
    command(0x11);
    delay(150);
    command(0x29);
    parkWindow();
    SPI.endTransaction();
    storage::unlock();
    return true;
}

bool drawFrame(uint16_t color)
{
    memset(row, 0, sizeof(row));
    for (unsigned x = 0; x < kWidth; ++x) {
        row[x * 2] = color >> 8;
        row[x * 2 + 1] = color & 0xFF;
    }
    if (!storage::lock(1000)) return false;
    const uint32_t start = micros();
    SPI.beginTransaction(SPISettings(tft_hz, MSBFIRST, SPI_MODE0));
    setWindow(0, 0, kWidth - 1, kHeight - 1);
    for (unsigned y = 0; y < kHeight; ++y) SPI.writeBytes(row, sizeof(row));
    parkWindow();
    SPI.endTransaction();
    const uint32_t elapsed = (micros() - start) / 1000;
    storage::unlock();
    frame_ms_total += elapsed;
    if (elapsed > frame_ms_max) frame_ms_max = elapsed;
    ++frames_done;
    return true;
}

uint8_t expected(size_t i)
{
    return uint8_t((i * 37u + i / 256u) ^ 0xA5u);
}

bool sdCycle()
{
    uint8_t block[256];
    char path[40];
    snprintf(path, sizeof(path), "/tftclk_%08lx.tmp", (unsigned long)millis());
    if (!storage::lock(1000)) {
        failure = "SD lock timeout";
        return false;
    }
    bool ok = !SD.exists(path);
    bool created = false;
    File f;
    if (ok) {
        f = SD.open(path, FILE_WRITE);
        created = bool(f);
        ok = created;
    }
    for (size_t offset = 0; ok && offset < kFileSize; offset += sizeof(block)) {
        for (size_t i = 0; i < sizeof(block); ++i) block[i] = expected(offset + i);
        ok = f.write(block, sizeof(block)) == sizeof(block);
    }
    if (f) { f.flush(); f.close(); }
    if (ok) {
        f = SD.open(path, FILE_READ);
        ok = bool(f) && f.size() == kFileSize;
    }
    for (size_t offset = 0; ok && offset < kFileSize; offset += sizeof(block)) {
        ok = f.read(block, sizeof(block)) == sizeof(block);
        for (size_t i = 0; ok && i < sizeof(block); ++i) ok = block[i] == expected(offset + i);
    }
    if (f) f.close();
    const bool removed = !created || SD.remove(path);
    storage::unlock();
    if (!ok || !removed) failure = !ok ? "SD write/readback" : "SD cleanup";
    else ++sd_passes;
    return ok && removed;
}

void report(const char *tag)
{
    const uint32_t mean = frames_done ? frame_ms_total / frames_done : 0;
    const char *result = failure[0] ? failure : running ? "running" :
                         strcmp(tag, "PASS") == 0 ? "pass" : "ready";
    Serial.printf("TFTSD %s tft_hz=%lu sd_hz=%lu frames=%u/%u mean_frame_ms=%lu max_frame_ms=%lu sd_verified=%u result=%s\n",
                  tag, (unsigned long)tft_hz, (unsigned long)kSdHz, frames_done,
                  kFramesPerRun, (unsigned long)mean, (unsigned long)frame_ms_max,
                  sd_passes, result);
}

void startRun(uint32_t hz)
{
    if (running) {
        Serial.println("TFTSD BUSY: wait for the current 32-frame run");
        return;
    }
    if (hz != 4000000 && hz != 6000000 && hz != 8000000 && hz != 10000000) {
        Serial.println("Use: clock 4|6|8|10");
        return;
    }
    tft_hz = hz;
    frame_ms_total = frame_ms_max = 0;
    frames_done = sd_passes = 0;
    failure = "";
    running = true;
    Serial.printf("TFTSD START tft_hz=%lu sd_hz=%lu frames=%u; panel max per Rev D: 10MHz\n",
                  (unsigned long)tft_hz, (unsigned long)kSdHz, kFramesPerRun);
}

void handleLine()
{
    line[line_size] = '\0';
    if (!line_overlong && strncmp(line, "clock ", 6) == 0) {
        const int mhz = atoi(line + 6);
        startRun(uint32_t(mhz) * 1000000u);
    } else if (!line_overlong && strcmp(line, "status") == 0) report("STATUS");
    else if (!line_overlong && strcmp(line, "help") == 0)
        Serial.println("Commands: clock 4|6|8|10 (start 32 full frames + SD write/read checks), status, help");
    line_size = 0;
    line_overlong = false;
}

void pollSerial()
{
    unsigned budget = 64;
    while (budget-- && Serial.available()) {
        const char c = Serial.read();
        if (c == '\n') handleLine();
        else if (c != '\r') {
            if (line_size < sizeof(line) - 1) line[line_size++] = c;
            else line_overlong = true;
        }
    }
}
} // namespace

void setup()
{
    Serial.begin(115200);
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);
    pinMode(kReset, OUTPUT);
    digitalWrite(kReset, LOW); /* Hold the no-CS panel reset during SD mount. */
    if (!storage::begin()) {
        Serial.println("TFTSD FAIL: SD mount at 8 MHz");
        while (true) delay(1000);
    }
    if (!initPanel()) {
        Serial.println("TFTSD FAIL: panel init");
        while (true) delay(1000);
    }
    Serial.println("TFTSD READY: stable no-CS panel; SD=8MHz. Run clock 4, then 6, 8, 10; inspect each pattern and SD result.");
    Serial.println("Use USB serial at 115200; remove any valuable card data before testing.");
}

void loop()
{
    pollSerial();
    if (!running) { delay(1); return; }
    const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    if (!drawFrame(colors[frames_done % 4])) failure = "display bus lock";
    if (!failure[0]) (void)sdCycle();
    if (failure[0] || frames_done >= kFramesPerRun) {
        running = false;
        report(failure[0] ? "FAIL" : "PASS");
    }
}
