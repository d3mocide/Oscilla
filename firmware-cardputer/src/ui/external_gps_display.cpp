/* Landscape GPS and SD activity view; Rev D §5–6. */
#include "ui/external_gps_display.h"

#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_SD_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
#include <Arduino.h>
#include <M5GFX.h>
#include <SPI.h>
#include <cstdio>
#include <cstring>
#include "storage/sd_storage.h"
#include "storage/sd_activity.h"
#include "ui/external_spectrum_view.h"
#include "ui/external_lora_view.h"
#include "ui/external_drive_view.h"
#include "ui/external_bt_view.h"
#include "ui/external_contacts_view.h"
#include "ui/external_zig_view.h"
#include "ui/theme.h"

namespace ui {
namespace {
constexpr int kDc = 6, kReset = 3, kSdCs = 12; /* Rev D §5. */
constexpr int kWidth = 320, kStripHeight = 24, kStrips = 10;
lgfx::LGFX_Sprite strip;
uint8_t row[kWidth * 2];
char lines[kStrips][48];
uint32_t last_frame, started_at;
bool frame_startup, frame_system_enabled, frame_zig_enabled, frame_pans;
size_t frame_zig_cursor;
model::ZigModel frame_zig;
model::SystemStatus frame_system;
int next_strip = kStrips;
bool enabled;
uint16_t state_colour;
model::SpectrumModel frame_spectrum;
size_t frame_cursor;
bool frame_monitor, frame_lora_enabled;
model::LoraModel frame_lora;
size_t frame_lora_cursor;
bool frame_drive_enabled;
model::DriveTrail frame_drive;
model::ContactsModel frame_contacts;
bool frame_contacts_enabled, frame_probes;
size_t frame_contacts_cursor;
model::BtModel frame_bt;
bool frame_bt_enabled;
size_t frame_bt_cursor;
model::GnssModel frame_gps;
storage::WardriveLogStats frame_log;
uint32_t frame_time;

void command(uint8_t cmd, const uint8_t *data = nullptr, size_t n = 0)
{
    digitalWrite(kDc, LOW);
    SPI.transfer(cmd);
    digitalWrite(kDc, HIGH);
    while (n--) SPI.transfer(*data++);
}

void sample(const model::GnssModel &gps, uint32_t bytes, uint32_t bad, uint32_t now)
{
    const auto state = gps.state(now);
    const bool fixed = state == model::GnssState::Fixed;
    state_colour = fixed ? TFT_GREEN : TFT_YELLOW;
    snprintf(lines[0], sizeof(lines[0]), "OSCILLA / GPS");
    snprintf(lines[1], sizeof(lines[1]), "%s", model::gnssStateName(state));
    snprintf(lines[2], sizeof(lines[2]), "RX bytes: %lu", (unsigned long)bytes);
    snprintf(lines[3], sizeof(lines[3]), "Bad checksum: %lu", (unsigned long)bad);
    snprintf(lines[4], sizeof(lines[4]), "UART: %s", gps.hasUartData(now) ? "receiving NMEA" : "no valid data");
    for (int i = 5; i < 9; ++i) lines[i][0] = '\0';
    if (fixed) {
        const auto &fix = gps.fix();
        snprintf(lines[5], sizeof(lines[5]), "Lat: %.5f", fix.lat_deg);
        snprintf(lines[6], sizeof(lines[6]), "Lon: %.5f", fix.lon_deg);
        snprintf(lines[7], sizeof(lines[7]), "HDOP: %.1f", fix.hdop);
        snprintf(lines[8], sizeof(lines[8]), "Fix age: %lus", (unsigned long)(gps.fixAgeMs(now) / 1000));
    } else {
        snprintf(lines[5], sizeof(lines[5]), "Waiting for fresh fix");
    }
#if defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY) && !defined(OSCILLA_SD_DISPLAY_BENCH) && !defined(OSCILLA_NO_CS_DISPLAY_BENCH)
    snprintf(lines[9], sizeof(lines[9]), "STORAGE");
#elif defined(OSCILLA_SD_DISPLAY_BENCH)
    snprintf(lines[9], sizeof(lines[9]), "BENCH / SD INTERLEAVE");
#else
    snprintf(lines[9], sizeof(lines[9]), "BENCH / SD DISABLED");
#endif
}
}

void holdExternalGpsDisplayReset()
{
    pinMode(kReset, OUTPUT);
    digitalWrite(kReset, LOW);
}

bool beginExternalGpsDisplay()
{
    strip.setColorDepth(16);
    if (!strip.createSprite(kWidth, kStripHeight)) return false;
    if (!storage::lock(20)) return false;
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
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    command(0x01);
    delay(150);
    const uint8_t format[] = {0x55}, orientation[] = {0x28};
    command(0x3A, format, sizeof(format));
    command(0x36, orientation, sizeof(orientation));
    command(0x11);
    delay(150);
    const uint8_t columns[] = {0, 0, 1, 63}, rows[] = {0, 0, 0, 239};
    command(0x2A, columns, sizeof(columns));
    command(0x2B, rows, sizeof(rows));
    command(0x2C);
    memset(row, 0, sizeof(row));
    for (int y = 0; y < 240; ++y) {
        SPI.writeBytes(row, sizeof(row));
        delay(1);
    }
    command(0x29);
#if defined(OSCILLA_SD_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
    const uint8_t idle_columns[] = {1, 63, 1, 63}, idle_rows[] = {0, 239, 0, 239};
    command(0x2A, idle_columns, sizeof(idle_columns));
    command(0x2B, idle_rows, sizeof(idle_rows));
    command(0x2C);
#endif
    SPI.endTransaction();
    storage::unlock();
    started_at=millis();
    enabled = true;
    last_frame = millis() - 1000;
    return true;
}

void tickExternalGpsDisplay(const model::GnssModel &gps, uint32_t bytes,
                            uint32_t checksum_failures, uint32_t now_ms,
                            const model::SpectrumModel *spectrum, size_t cursor,
                            const model::LoraModel *lora, size_t lora_cursor, const model::DriveTrail *drive,
                            const model::BtModel *bt, size_t bt_cursor,
                            const model::ContactsModel *contacts, bool probes, size_t contacts_cursor,
                            const model::ZigModel *zig, bool pans, size_t zig_cursor,
                            const model::SystemStatus *system)
{
    if (!enabled) return;
    if (next_strip == kStrips) {
        if (now_ms - last_frame < 1000) return;
        sample(gps, bytes, checksum_failures, now_ms);
        frame_monitor = spectrum != nullptr;
        frame_lora_enabled = lora != nullptr;
        frame_startup=now_ms-started_at<2000;
        frame_system_enabled=system!=nullptr;
        if(system) frame_system=*system;
        frame_zig_enabled=zig!=nullptr;
        if(zig) frame_zig=*zig;
        frame_pans=pans;frame_zig_cursor=zig_cursor;
        frame_contacts_enabled = contacts != nullptr;
        if(contacts) frame_contacts=*contacts;
        frame_probes=probes;
        frame_contacts_cursor=contacts_cursor;
        frame_bt_enabled = bt != nullptr;
        if(bt) frame_bt=*bt;
        frame_bt_cursor=bt_cursor;
        frame_drive_enabled = drive != nullptr;
        if(drive) {
            frame_drive=*drive;frame_gps=gps;
#if !defined(OSCILLA_SD_DISPLAY_BENCH)
            frame_log=storage::wardriveLogStats();
#endif
        }
        if (lora) frame_lora = *lora;
        frame_lora_cursor = lora_cursor;
        if (spectrum) frame_spectrum = *spectrum;
        frame_cursor = cursor;
        frame_time = now_ms;
        last_frame = now_ms;
        next_strip = 0;
    }
    strip.fillScreen(TFT_BLACK);
    strip.setTextSize(2);
    strip.setTextColor(next_strip == 1 ? state_colour : TFT_WHITE, TFT_BLACK);
    strip.setCursor(8, 4);
    strip.print(lines[next_strip]);
    if(frame_startup) {
        drawExternalStartupStrip(strip,next_strip*kStripHeight);
    } else if(frame_system_enabled) {
        drawExternalSystemStrip(strip,next_strip*kStripHeight,frame_system,frame_time);
    } else if(frame_zig_enabled) {
        drawExternalZigStrip(strip,next_strip*kStripHeight,frame_zig,frame_pans,frame_zig_cursor);
    } else if(frame_contacts_enabled) {
        drawExternalContactsStrip(strip,next_strip*kStripHeight,frame_contacts,frame_probes,frame_contacts_cursor,frame_time);
    } else if(frame_bt_enabled) {
        drawExternalBtStrip(strip,next_strip*kStripHeight,frame_bt,frame_bt_cursor);
    } else if(frame_drive_enabled) {
        drawExternalDriveStrip(strip,next_strip*kStripHeight,frame_gps,frame_drive,frame_log,frame_time);
    } else if (frame_lora_enabled) {
        drawExternalLoraStrip(strip, next_strip * kStripHeight, frame_lora, frame_lora_cursor, frame_time);
    } else if (frame_monitor) {
        strip.fillScreen(kVoidInk);
        drawExternalSpectrumStrip(strip, next_strip * kStripHeight, frame_spectrum, frame_cursor, frame_time);
    }
#if defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY) && !defined(OSCILLA_SD_DISPLAY_BENCH) && !defined(OSCILLA_NO_CS_DISPLAY_BENCH)
    if (next_strip == kStrips - 1) {
        const auto &activity = storage::sdActivity();
        const auto state = activity.indicator(storage::ready(), now_ms);
        const uint16_t colour = state == storage::SdIndicator::Fault ? kFaultRed :
                                state == storage::SdIndicator::Write ? kFieldGreen : kPanelDark;
        strip.fillRect(232, 0, 88, kStripHeight, kPanelDark);
        strip.drawRect(232, 0, 88, kStripHeight, kLineBorder);
        strip.fillRect(306, 5, 8, 8, colour);
        strip.setTextSize(1);
        strip.setTextColor(kPaperPhosphor, kPanelDark);
        strip.setCursor(238, 5);
        strip.print(storage::ready() ? "SD" : "SD OFF");
        strip.setCursor(238, 14);
        strip.printf("W %lu", (unsigned long)activity.writes);
    }
#endif
    if (!storage::lock(2)) return;
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    const uint8_t columns[] = {0, 0, 1, 63};
    const uint8_t y0 = next_strip * kStripHeight, y1 = y0 + kStripHeight - 1;
    const uint8_t rows[] = {0, y0, 0, y1};
    command(0x2A, columns, sizeof(columns));
    command(0x2B, rows, sizeof(rows));
    command(0x2C);
    for (int y = 0; y < kStripHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const uint16_t pixel = strip.readPixel(x, y);
            row[x * 2] = pixel >> 8;
            row[x * 2 + 1] = pixel & 0xFF;
        }
        SPI.writeBytes(row, sizeof(row));
    }
#if defined(OSCILLA_SD_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
    /* Park before releasing the shared lock: SD data lands in the corner. */
    const uint8_t idle_columns[] = {1, 63, 1, 63}, idle_rows[] = {0, 239, 0, 239};
    command(0x2A, idle_columns, sizeof(idle_columns));
    command(0x2B, idle_rows, sizeof(idle_rows));
    command(0x2C);
#endif
    SPI.endTransaction();
    storage::unlock();
    ++next_strip;
}
}
#endif
