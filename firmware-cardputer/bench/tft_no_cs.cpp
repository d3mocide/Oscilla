/* Isolated ILI9341 test; remove SD. Pin authority: Rev D §5. */
#include <Arduino.h>
#include <SPI.h>
#include <M5GFX.h>
#include "gnss/nmea_parser.h"
#include "model/gnss_model.h"

static constexpr int kClock = 40, kMosi = 14, kDc = 6, kReset = 3;
static constexpr int kSdCs = 12;
static gnss::NmeaParser parser;
static model::GnssModel gps;
static lgfx::LGFX_Sprite canvas;
static uint32_t bytesReceived, sentencesReceived, lastSentenceMs, lastDrawMs;

static void pollGps()
{
    uint8_t bytes[128];
    size_t n = 0;
    while (Serial2.available() && n < sizeof(bytes)) bytes[n++] = Serial2.read();
    bytesReceived += n;
    parser.feed(bytes, n, [](const gnss::Sentence &sentence) {
        lastSentenceMs = millis();
        ++sentencesReceived;
        gps.absorb(sentence, lastSentenceMs);
    });
}

static void command(uint8_t cmd, const uint8_t *data = nullptr, size_t n = 0)
{
    digitalWrite(kDc, LOW);
    SPI.transfer(cmd);
    digitalWrite(kDc, HIGH);
    while (n--) SPI.transfer(*data++);
}

void setup()
{
    Serial.begin(115200);
    Serial2.setRxBufferSize(2048);
    Serial2.begin(9600, SERIAL_8N1, 15, -1); /* Receive only; Rev D §6. */
    pinMode(kSdCs, OUTPUT);
    digitalWrite(kSdCs, HIGH);
    pinMode(kDc, OUTPUT);
    pinMode(kReset, OUTPUT);
    digitalWrite(kReset, HIGH);
    delay(10);
    digitalWrite(kReset, LOW);
    delay(20);
    digitalWrite(kReset, HIGH);
    delay(150);
    SPI.begin(kClock, -1, kMosi, -1);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    command(0x01);
    delay(150);
    const uint8_t format[] = {0x55};
    command(0x3A, format, sizeof(format));
    const uint8_t orientation[] = {0x28};
    command(0x36, orientation, sizeof(orientation));
    command(0x11);
    delay(150);
    command(0x29);
    delay(20);
    const uint8_t columns[] = {0, 0, 1, 63};
    const uint8_t rows[] = {0, 0, 0, 239};
    command(0x2A, columns, sizeof(columns));
    command(0x2B, rows, sizeof(rows));
    command(0x2C);
    const uint16_t colours[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    for (unsigned y = 0; y < 240; ++y) {
        const uint16_t colour = colours[y / 60];
        for (unsigned x = 0; x < 320; ++x) {
            SPI.transfer(colour >> 8);
            SPI.transfer(colour & 0xFF);
        }
    }
    SPI.endTransaction();
    Serial.println("ILI9341 pattern sent; inspect external screen. No readback proof.");
    canvas.setColorDepth(16);
    if (!canvas.createSprite(320, 240)) {
        Serial.println("GPS screen allocation failed");
        while (true) delay(1000);
    }
}

void loop()
{
    pollGps();
    const uint32_t now = millis();
    if (now - lastDrawMs < 1000) {
        delay(1);
        return;
    }
    lastDrawMs = now;
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextSize(2);
    canvas.setCursor(8, 12);
    canvas.println("OSCILLA GPS TEST");
    canvas.println("SD REMOVED");
    canvas.println();
    canvas.setTextColor(gps.state(now) == model::GnssState::Fixed ? TFT_GREEN : TFT_YELLOW);
    canvas.println(model::gnssStateName(gps.state(now)));
    canvas.setTextColor(TFT_WHITE);
    canvas.printf("Bytes: %lu\n", (unsigned long)bytesReceived);
    canvas.printf("NMEA: %lu\n", (unsigned long)sentencesReceived);
    canvas.printf("Bad sum: %lu\n", (unsigned long)parser.checksumFailures());
    if (sentencesReceived) canvas.printf("Data age: %lus\n", (unsigned long)((now-lastSentenceMs)/1000));
    if (gps.state(now) == model::GnssState::Fixed) {
        canvas.printf("Lat: %.5f\nLon: %.5f\n", gps.fix().lat_deg, gps.fix().lon_deg);
        canvas.printf("HDOP: %.1f\n", gps.fix().hdop);
        canvas.printf("Fix age: %lus\n", (unsigned long)(gps.fixAgeMs(now)/1000));
    }
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    const uint8_t columns[] = {0, 0, 1, 63}, rows[] = {0, 0, 0, 239};
    command(0x2A, columns, sizeof(columns));
    command(0x2B, rows, sizeof(rows));
    command(0x2C);
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            uint16_t pixel = canvas.readPixel(x, y);
            SPI.transfer(pixel >> 8);
            SPI.transfer(pixel & 0xFF);
        }
        pollGps();
        delay(1);
    }
    SPI.endTransaction();
    Serial.printf("GPS bytes=%lu nmea=%lu state=%s checksum_fail=%lu\n",
                  (unsigned long)bytesReceived, (unsigned long)sentencesReceived,
                  model::gnssStateName(gps.state(millis())),
                  (unsigned long)parser.checksumFailures());
}
