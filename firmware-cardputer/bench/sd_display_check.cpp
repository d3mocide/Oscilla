/* Experimental SD/GPS/display coexistence fixture; Rev D §5–6. */
#include <M5Cardputer.h>
#include <SD.h>
#include <cstring>
#include "gnss/nmea_parser.h"
#include "model/gnss_model.h"
#include "storage/sd_storage.h"
#include "ui/external_gps_display.h"

namespace {
enum class Phase { Waiting, Writing, Reading, Passed, Failed };
Phase phase = Phase::Waiting;
gnss::NmeaParser parser;
model::GnssModel gps;
File file;
char path[48];
uint8_t block[256];
uint32_t received, last_step, last_draw;
uint32_t last_status;
char usb_line[32];
size_t usb_length;
bool usb_overlong;
unsigned chunk;
bool created, external_ready, cleaned;
const char *failure = "";
constexpr unsigned kCycles = 32;
unsigned cycles_passed, write_ops;
uint32_t written_bytes, verified_bytes, settle_at;
bool settling;
M5Canvas screen(&M5Cardputer.Display);

const char *phaseName()
{
    switch (phase) {
    case Phase::Waiting: return "waiting";
    case Phase::Writing: return "writing";
    case Phase::Reading: return "reading";
    case Phase::Passed: return "passed";
    case Phase::Failed: return "failed";
    }
    return "invalid";
}

void status()
{
    Serial.printf("SD/display status phase=%s block=%u/16 cycles=%u/%u write_ops=%u written_bytes=%lu verified_bytes=%lu cleanup=%s external=%s gps=%s rx_bytes=%lu checksum_fail=%lu reason=%s\n",
                  phaseName(), chunk, cycles_passed, kCycles, write_ops,
                  (unsigned long)written_bytes, (unsigned long)verified_bytes,
                  cleaned ? "removed" : created ? "pending" : "none",
                  external_ready ? "initialized" : "not initialized",
                  model::gnssStateName(gps.state(millis())), (unsigned long)received,
                  (unsigned long)parser.checksumFailures(), failure);
}

uint8_t expected(unsigned offset)
{
    return static_cast<uint8_t>((offset * 37u + offset / 256u) ^ 0xA5u);
}

void finish(bool ok, const char *reason)
{
    /* Caller holds the shared bus lock. */
    file.close();
    if (created) cleaned = SD.remove(path);
    if (ok && !cleaned) { ok = false; reason = "cleanup failed"; }
    if (ok) ++cycles_passed;
    phase = ok ? Phase::Passed : Phase::Failed;
    failure = reason;
    Serial.printf("SD/display %s reason=%s cleanup=%s bytes_checked=%u\n",
                  ok ? "PASS" : "FAIL", reason, cleaned ? "removed" : "not removed",
                  phase == Phase::Passed ? 4096u : chunk * 256u);
}

void openCycle()
{
    if (!storage::lock(20)) {
        phase = Phase::Failed;
        failure = "SD lock";
        return;
    }
    created = false;
    cleaned = false;
    chunk = 0;
    failure = "";
    snprintf(path, sizeof(path), "/oscilla_coexist_%08lx.tmp", (unsigned long)millis());
    if (SD.exists(path)) {
        finish(false, "file already exists");
    } else {
        file = SD.open(path, FILE_WRITE);
        created = static_cast<bool>(file);
        if (!created) finish(false, "file create");
        else phase = Phase::Writing;
    }
    storage::unlock();
    last_step = millis();
}

void start()
{
    if (!storage::begin()) {
        phase = Phase::Failed;
        failure = "SD mount";
        return;
    }
    external_ready = ui::beginExternalGpsDisplay();
    if (!external_ready) {
        phase = Phase::Failed;
        failure = "external init";
        return;
    }
    phase = Phase::Writing;
    settle_at = millis();
    settling = true; /* First full GPS redraw precedes SD traffic. */
}

void pollUsb()
{
    unsigned budget = 64;
    while (budget-- && Serial.available()) {
        const char c = Serial.read();
        if (c == '\n') {
            usb_line[usb_length] = '\0';
            if (!usb_overlong) {
                if (!strcmp(usb_line, "start") && phase == Phase::Waiting) start();
                if (!strcmp(usb_line, "status")) status();
            }
            usb_length = 0;
            usb_overlong = false;
        } else if (c != '\r') {
            if (usb_length < sizeof(usb_line) - 1) usb_line[usb_length++] = c;
            else usb_overlong = true;
        }
    }
}

void step(uint32_t now)
{
    if (settling) {
        if (now - settle_at < 1500) return;
        settling = false;
        openCycle();
        return;
    }
    if (phase == Phase::Passed && cycles_passed < kCycles) {
        openCycle();
        return;
    }
    if (phase != Phase::Writing && phase != Phase::Reading) return;
    if (now - last_step < 250 || !storage::lock(2)) return;
    last_step = now;
    if (phase == Phase::Writing) {
        for (unsigned i = 0; i < sizeof(block); ++i) block[i] = expected(chunk * sizeof(block) + i);
        if (file.write(block, sizeof(block)) != sizeof(block)) {
            finish(false, "short write");
        } else {
            ++write_ops;
            written_bytes += sizeof(block);
            file.flush();
            if (++chunk == 16) {
                file.close();
                file = SD.open(path, FILE_READ);
                if (!file || file.size() != 4096) finish(false, "reopen/length");
                else { chunk = 0; phase = Phase::Reading; }
            }
        }
    } else {
        bool ok = file.read(block, sizeof(block)) == sizeof(block);
        for (unsigned i = 0; i < sizeof(block) && ok; ++i) {
            ok = block[i] == expected(chunk * sizeof(block) + i);
        }
        if (!ok) finish(false, "read mismatch");
        else {
            verified_bytes += sizeof(block);
            if (++chunk == 16) finish(true, "4096 bytes verified");
        }
    }
    storage::unlock();
}

void draw()
{
    auto &d = screen;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(phase == Phase::Failed ? TFT_RED : TFT_WHITE, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(8, 4);
    d.println("SD WRITE OPS");
    d.setTextSize(5);
    d.setCursor(8, 24);
    d.printf("%u", write_ops);
    d.setTextSize(1);
    d.setCursor(8, 70);
    d.printf("Written %lu B / verified %lu B\n", (unsigned long)written_bytes, (unsigned long)verified_bytes);
    d.setCursor(8, 82);
    d.printf("Cycles %u/%u / %s %u/16\n", cycles_passed, kCycles, phaseName(), chunk);
    d.setCursor(8, 94);
    if (phase == Phase::Waiting) d.print("ENTER once or USB start");
    else if (phase == Phase::Failed) d.printf("FAIL: %s", failure);
    else if (phase == Phase::Passed && cycles_passed == kCycles) d.print("PASS / all test files removed");
    else if (settling) d.print("Preparing GPS display...");
    else d.print("256-byte writes, then readback");
    d.setCursor(8, 106);
    d.printf("GPS %s / RX %lu", model::gnssStateName(gps.state(millis())), (unsigned long)received);
    d.setCursor(8, 118);
    d.printf("Checksum errors %lu", (unsigned long)parser.checksumFailures());
    d.pushSprite(0, 0);
}
}

void setup()
{
    Serial.begin(115200);
    M5Cardputer.begin(M5.config(), true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    screen.setColorDepth(16);
    if (!screen.createSprite(240, 135)) {
        Serial.println("SD/display FAIL: internal canvas allocation");
        while (true) delay(1000);
    }
    Serial2.setRxBufferSize(2048);
    Serial2.begin(9600, SERIAL_8N1, 15, -1);
    draw();
    Serial.println("SD/display ready: ENTER or USB 'start'; USB 'status' reports current state");
}

void loop()
{
    uint8_t bytes[128];
    size_t n = 0;
    while (Serial2.available() && n < sizeof(bytes)) bytes[n++] = Serial2.read();
    received += n;
    parser.feed(bytes, n, [](const gnss::Sentence &s) { gps.absorb(s, millis()); });
    M5Cardputer.update();
    pollUsb();
    if (phase == Phase::Waiting && M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed() && M5Cardputer.Keyboard.keysState().enter) start();
    step(millis());
    if (external_ready) ui::tickExternalGpsDisplay(gps, received, parser.checksumFailures(), millis());
    if (millis() - last_draw >= 500) { last_draw = millis(); draw(); }
    if (millis() - last_status >= 1000) { last_status = millis(); status(); }
    delay(1);
}
