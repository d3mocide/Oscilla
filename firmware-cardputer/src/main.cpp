/*
 * main.cpp — oscilla-cp (the deck). Wiring only: M5 hardware, the Grove link,
 * keyboard and the app.
 *
 * SPDX-License-Identifier: MIT
 */

#include <M5Cardputer.h>

#include "app/deck_app.h"
#include "debug/line_reader.h"
#include "ocp.h"
#include "storage/sd_storage.h"
#include "storage/settings.h"
#include "ui/boot_screen.h"
#include "ui/canvas.h"
#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
#include "ui/external_gps_display.h"
#endif
#include "storage/sd_activity.h"
#include "storage/lora_logger.h"
#include "storage/wardrive_logger.h"

namespace {

/* Rev D §3. Start Serial1 after M5.begin(): Port A shares these pins. */
constexpr int kGroveTxPin = 2;
constexpr int kGroveRxPin = 1;

/* Rev D §6: a distinct hardware UART from Grove. TX 13 / RX 15 are the
 * host's own pins; 9600 8N1 is the -5N family default, not guaranteed for a
 * preconfigured unit — no settings UI yet to change it at runtime, so this
 * is the one place to edit until P4 grows one. */
constexpr int kGnssTxPin = 13;
constexpr int kGnssRxPin = 15;
constexpr uint32_t kGnssBaudDefault = 9600;
constexpr uint32_t kBootScreenHoldMs = 3000;

app::DeckApp g_app([](const char *data, size_t len) {
    Serial1.write(reinterpret_cast<const uint8_t *>(data), len);
});

/* Debug console line reassembly ('d' toggles debug mode at runtime,
 * DeckApp::onKeys). Lives here, not in DeckApp, for the same reason the
 * OCP/GNSS parsers stay out of it: byte-chunking is a wiring concern,
 * command semantics aren't. */
debug::LineReader g_debug_reader;
#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
uint32_t g_gnss_bytes;
#endif

void logDeckMemory(const char *tag, uint32_t uptime_ms);

}  // namespace

void setup()
{
    Serial.begin(115200);   /* USB: deck diagnostics only */
    M5Cardputer.begin(M5.config(), true);
#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
    ui::holdExternalGpsDisplayReset();
#endif
    M5Cardputer.Display.setRotation(1);
    ui::initCanvas();
    ui::drawBootScreen();
    delay(kBootScreenHoldMs);

    /* Shared bus first; no-CS bench disables SD mounting (Rev D §5.2). */
    if (!storage::begin()) Serial.println("deck: no SD card, logging unavailable");
#ifdef OSCILLA_NO_CS_DISPLAY_BENCH
    Serial.println("deck bench: SD disabled for no-CS panel");
#endif

    /* Brightness (DESIGN §7.6): read only now that SD is up (must follow
     * storage::begin() above), same "SD remembers it, not the firmware
     * image" shape as debug mode below. No-ops to
     * storage::settings.cpp's kBrightnessDefault if there's no card yet or
     * no saved value. The boot screen above therefore draws at the
     * hardware's own power-on brightness, not the saved one — unavoidable,
     * since the card can't be read before it's mounted. */
    {
        uint8_t brightness = storage::loadBrightness();
        M5Cardputer.Display.setBrightness(brightness);
        g_app.setBrightness(brightness);
    }

    /* Debug mode (DESIGN §7.6): a runtime toggle ('d', DeckApp::onKeys), not
     * a boot gesture — an earlier boot-hold design didn't survive contact
     * with the ADV's keyboard hardware (WORKLOG 2026-09-16: the TCA8418
     * reader is edge-driven and flushes its FIFO in begin(), so a key
     * already down before that point never fires a new edge). Reading the
     * persisted flag here is what makes it survive a reflash: the SD card
     * remembers it, not the firmware image. No-ops to false if there's no
     * card yet. */
    g_app.setDebugMode(storage::loadDebugMode());
    if (g_app.debugEnabled()) Serial.println("deck: debug mode on (persisted)");

    /* A 256-row [SCAN] page is ~20 KB in ~2 s; a redraw must not overflow the buffer. */
    Serial1.setRxBufferSize(16384);
    Serial1.begin(OCP_BAUD_DEFAULT, SERIAL_8N1, kGroveRxPin, kGroveTxPin);

    /* Own UART, own baud: GNSS never crosses OCP (DESIGN §9.1), so it has
     * nothing to do with the Grove link's proto or framing. Buffer sized
     * well past the Arduino core's 256 B default (2026-09-16, WORKLOG): at
     * 9600 baud that's only ~266 ms of slack against any stall elsewhere in
     * loop() (e.g. an SD flush on the shared SPI bus, Rev D §5.2), and a
     * silent overflow there is indistinguishable from a marginal fix without
     * the error callback below. */
    Serial2.setRxBufferSize(2048);
    Serial2.onReceiveError([](hardwareSerial_error_t err) {
        Serial.printf("deck gnss uart error=%d\n", static_cast<int>(err));
    });
    Serial2.begin(kGnssBaudDefault, SERIAL_8N1, kGnssRxPin, kGnssTxPin);
#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
    if (!ui::beginExternalGpsDisplay()) Serial.println("deck: external display init failed");
    else Serial.println("deck: external GPS display ready; SD configured at 8 MHz");
#endif

    g_app.onLog([](const std::string &line) { Serial.printf("deck %s\n", line.c_str()); });
    g_app.begin(millis());
    logDeckMemory("boot", millis());
}

namespace {
/* Diagnostic only, not tied to any view: a slow leak shows up as a trend in
 * these numbers over hours, not as a crash. The ADV has no populated PSRAM;
 * the zero-valued PSRAM fields make that a measured result rather than an
 * assumption about the board SKU. */
constexpr uint32_t kHeapLogMs = 5UL * 60UL * 1000UL;
uint32_t g_last_heap_log_ms = 0;

void logDeckMemory(const char *tag, uint32_t uptime_ms)
{
    Serial.printf("deck memory tag=%s heap_free=%u heap_min=%u heap_largest=%u psram_total=%u psram_free=%u psram_largest=%u uptime_s=%lu\n",
                  tag,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMinFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap(),
                  (unsigned)ESP.getPsramSize(),
                  (unsigned)ESP.getFreePsram(),
                  (unsigned)ESP.getMaxAllocPsram(),
                  (unsigned long)(uptime_ms / 1000));
}
}  // namespace

void loop()
{
    uint32_t now = millis();

    if (now - g_last_heap_log_ms >= kHeapLogMs) {
        g_last_heap_log_ms = now;
        logDeckMemory("periodic", now);
    }

    /* Drain the link completely before any drawing. */
    uint8_t buf[512];
    size_t n = 0, total = 0;
    do {
        n = 0;
        while (Serial1.available() && n < sizeof buf) buf[n++] = Serial1.read();
        if (n) g_app.feed(buf, n, now);
        total += n;
    } while (n == sizeof buf);
    g_app.tick(now);

    /* GNSS: same drain-completely-before-drawing shape, independent UART.
     * The parser and fix model live in DeckApp (the Drive card reads them);
     * this loop only moves bytes. */
    while (Serial2.available()) {
        uint8_t gbuf[128];
        size_t gn = 0;
        while (Serial2.available() && gn < sizeof gbuf) gbuf[gn++] = Serial2.read();
        g_app.feedGnss(gbuf, gn, now);
#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
        g_gnss_bytes += gn;
#endif
    }

    /* Debug console: drained unconditionally so stray bytes never pile up
     * in Serial's own RX buffer, but runDebugCommand() no-ops unless 'd'
     * has toggled debug mode on (deck_app.cpp). */
    while (Serial.available()) {
        uint8_t dbuf[64];
        size_t dn = 0;
        while (Serial.available() && dn < sizeof dbuf) dbuf[dn++] = Serial.read();
        g_debug_reader.feed(dbuf, dn, [&](const std::string &line) {
            if (line == "debug on" || line == "debug off") {
                g_app.setDebugMode(line == "debug on");
                Serial.println(g_app.debugEnabled() ? "deck debug: enabled in RAM" : "deck debug: disabled in RAM");
            } else if (line == "display_status") {
                const auto &a = storage::sdActivity();
                const auto &l = storage::loraLogStats();
                const auto &w = storage::wardriveLogStats();
                Serial.printf("deck display_status sd_ready=%u writes=%lu bytes=%llu write_errors=%lu lora_open=%u packet_rows=%lu packet_drops=%lu health_rows=%lu health_drops=%lu wardrive_open=%u ap_rows=%lu nofix=%lu gnss=%s\n",
                              storage::ready(), (unsigned long)a.writes, (unsigned long long)a.bytes,
                              (unsigned long)a.errors, l.open, (unsigned long)l.packet_rows,
                              (unsigned long)l.packet_drops, (unsigned long)l.health_rows,
                              (unsigned long)l.health_drops, w.open, (unsigned long)w.aps,
                              (unsigned long)w.aps_no_fix, model::gnssStateName(g_app.gnssModel().state(now)));
            } else if (line == "verify_lora_log" && g_app.debugEnabled()) {
                const auto v = storage::verifyLoraLog();
                const char *state = v.state == storage::LoraReadbackState::Match ? "match" :
                                    v.state == storage::LoraReadbackState::Mismatch ? "mismatch" :
                                    v.state == storage::LoraReadbackState::TooLarge ? "limit" : "unavailable";
                Serial.printf("deck log_readback state=%s packet_bytes=%lu health_bytes=%lu\n",
                              state, (unsigned long)v.packet_bytes, (unsigned long)v.health_bytes);
            } else if (line == "memory") {
                if (g_app.debugEnabled()) {
                    logDeckMemory("command", now);
                    Serial.println("deck debug: ok memory");
                }
            } else {
                g_app.runDebugCommand(line, now);
            }
        });
    }

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const auto &ks = M5Cardputer.Keyboard.keysState();
        app::Keys keys;
        keys.chars.assign(ks.word.begin(), ks.word.end());
        keys.enter = ks.enter;
        g_app.onKeys(keys, now);
    }

    if (g_app.dirty(now)) g_app.draw(now);
#if defined(OSCILLA_NO_CS_DISPLAY_BENCH) || defined(OSCILLA_EXTERNAL_NO_CS_DISPLAY)
    static model::SystemStatus external_system;
    static uint32_t last_system_sample=0;
    if(g_app.showingSystem() && (now-last_system_sample>=1000 || external_system.info!=g_app.showingInfo())) {
        external_system=g_app.externalSystemState();
        external_system.deck_heap=ESP.getFreeHeap();external_system.deck_min=ESP.getMinFreeHeap();
        last_system_sample=now;
    }
    ui::tickExternalGpsDisplay(g_app.gnssModel(), g_gnss_bytes,
                               g_app.gnssChecksumFailures(), millis(),
                               g_app.showingSpectrum() ? &g_app.spectrumModel() : nullptr,
                               g_app.spectrumCursor(),
                               g_app.showingLora() ? &g_app.loraModel() : nullptr,
                               g_app.loraCursor(),
                               g_app.showingDrive() ? &g_app.driveTrail() : nullptr,
                               g_app.showingBt() ? &g_app.btModel() : nullptr,
                               g_app.btCursor(),
                               g_app.showingContacts() ? &g_app.contactsModel() : nullptr,
                               g_app.contactsProbes(), g_app.contactsCursor(),
                               g_app.showingZig() ? &g_app.zigModel() : nullptr,
                               g_app.zigPans(), g_app.zigCursor(),
                               g_app.showingSystem() ? &external_system : nullptr);
#endif

    if (!total) delay(1);   /* yield when idle */
}
