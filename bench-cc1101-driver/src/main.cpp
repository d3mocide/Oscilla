// bench-cc1101-driver — Oscilla's CC1101 layers on a XIAO ESP32-S3 (D-15 A/B).
// Receive-only. Same line format as bench-cc1101-s3 so its tools work unchanged.
#include <Arduino.h>
#include <SPI.h>
#include <driver/gpio.h>

extern "C" {
#include "cc1101_bus.h"
#include "cc1101_config.h"
#include "cc1101_profile.h"
#include "cc1101_rx.h"
}

// Bench wiring only (same XIAO labels as the archived C5 harness), not Rev D.
constexpr int kSck = 7;    // D8
constexpr int kMiso = 8;   // D9
constexpr int kMosi = 9;   // D10
constexpr int kCsn = 4;    // D3
constexpr int kGdo0 = 2;   // D1
constexpr int kVcc = 5;    // D4: optional CC1101 supply for automated power cycling

constexpr uint32_t kWindowMs = 20;
constexpr uint32_t kVerifyMs = 1000;

static const SPISettings kSpi(1000000, MSBFIRST, SPI_MODE0);

static bool tx_xfer(void *, const uint8_t *tx, uint8_t *rx, size_t len)
{
    SPI.beginTransaction(kSpi);
    for (size_t i = 0; i < len; i++) rx[i] = SPI.transfer(tx[i]);
    SPI.endTransaction();
    return true;
}
static void tx_cs(void *, bool asserted) { digitalWrite(kCsn, asserted ? LOW : HIGH); }
static int tx_so(void *) { return digitalRead(kMiso); }
static void tx_delay(void *, uint32_t us) { delayMicroseconds(us); }
static uint32_t tx_now(void *) { return micros(); }

static const cc1101_bus_t g_bus = { nullptr, tx_xfer, tx_cs, tx_so, tx_delay, tx_now };
static cc1101_profile_t g_profile;
static volatile uint32_t g_edges = 0;
static uint32_t g_drift_count = 0;
static char g_bringup[200];
static uint32_t g_gap_us = CC1101_RSSI_POLL_MIN_US;   // 'gap N': SPI-coupling test
static bool g_spi_quiet = false;  // 'quiet': no SPI at all; GDO0 edges still counted

static void IRAM_ATTR onGdo0() { g_edges++; }

static void printInfo()
{
    uint8_t part = 0, ver = 0;
    const cc1101_err_t perr = cc1101_probe(&g_bus, &part, &ver);
    uint8_t f2 = 0, f1 = 0, f0 = 0;
    cc1101_read_reg(&g_bus, CC1101_FREQ2, &f2);
    cc1101_read_reg(&g_bus, CC1101_FREQ1, &f1);
    cc1101_read_reg(&g_bus, CC1101_FREQ0, &f0);
    const uint32_t freq = (uint32_t(f2) << 16) | (uint32_t(f1) << 8) | f0;
    Serial.printf("I,partnum=0x%02x,version=0x%02x,freq_reg=0x%06lx,mhz_at_26=%.4f,probe=%s,"
                  "drifts=%lu,driver=oscilla,%s\n",
                  part, ver, (unsigned long)freq, freq * 26.0 / 65536.0, cc1101_err_name(perr),
                  (unsigned long)g_drift_count, g_bringup);
}

// Full bring-up through the driver: probe, reset, apply, rx_start.
static void bringUp(bool do_reset)
{
    bool edge = false, cal = false;
    cc1101_apply_report_t r = { 0, 0, 0, -1 };
    const cc1101_err_t pr = cc1101_probe(&g_bus, nullptr, nullptr);
    const cc1101_err_t rs = do_reset ? cc1101_reset(&g_bus, &edge) : CC1101_OK;
    const cc1101_err_t ap = cc1101_apply_profile(&g_bus, &g_profile, &r);
    const cc1101_err_t rx = ap == CC1101_OK ? cc1101_rx_start(&g_bus, &cal) : ap;
    snprintf(g_bringup, sizeof g_bringup,
             "probe=%s,reset=%s,so_edge=%d,apply=%s,written=%u,retried=%u,failed=%u,first_failed=%d,"
             "rx=%s,calibrated=%d",
             cc1101_err_name(pr), do_reset ? cc1101_err_name(rs) : "skipped", edge,
             cc1101_err_name(ap), r.written, r.retried, r.failed, r.first_failed,
             cc1101_err_name(rx), cal);
}

// Stress: N full bring-ups (reset, apply, rx_start) or N rx stop/start pairs.
static void stressCycles(bool full, long n)
{
    long fail = 0, no_edge = 0, retried = 0;
    char first[96] = "";
    const uint32_t t0 = millis();
    for (long i = 0; i < n; i++) {
        cc1101_err_t err;
        if (full) {
            bool edge = false, cal = false;
            cc1101_apply_report_t r = { 0, 0, 0, -1 };
            err = cc1101_reset(&g_bus, &edge);
            if (err == CC1101_OK) err = cc1101_apply_profile(&g_bus, &g_profile, &r);
            if (err == CC1101_OK) err = cc1101_rx_start(&g_bus, &cal);
            if (err == CC1101_OK && !cal) err = CC1101_ERR_STATE;
            no_edge += !edge;
            retried += r.retried;
            if (err != CC1101_OK && !first[0])
                snprintf(first, sizeof first, "i=%ld,err=%s,failed=%u,first_failed=%d", i,
                         cc1101_err_name(err), r.failed, r.first_failed);
        } else {
            bool cal = false;
            err = cc1101_rx_stop(&g_bus);
            if (err == CC1101_OK) err = cc1101_rx_start(&g_bus, &cal);
            if (err != CC1101_OK && !first[0])
                snprintf(first, sizeof first, "i=%ld,err=%s", i, cc1101_err_name(err));
        }
        fail += err != CC1101_OK;
        if ((i & 15) == 0) delay(1);   // let USB/serial breathe
    }
    int16_t at = -1;
    const cc1101_err_t v = cc1101_verify_profile(&g_bus, &g_profile, &at);
    uint8_t marc = 0;
    cc1101_read_status(&g_bus, CC1101_MARCSTATE, &marc);
    Serial.printf("S,%s,n=%ld,fail=%ld,no_so_edge=%ld,reg_retries=%ld,ms=%lu,verify=%s,marc=%02x,first_fail=%s\n",
                  full ? "bringup" : "rxcycle", n, fail, no_edge, retried,
                  (unsigned long)(millis() - t0), cc1101_err_name(v), marc, first[0] ? first : "none");
    if (v != CC1101_OK || marc != CC1101_MARC_RX) { bringUp(false); printInfo(); }
}

// Automated power cycling with VCC on kVcc. While off, every CC1101 line is an
// input with pull-down so nothing back-powers the chip through its ESD diodes.
static bool g_vcc_gpio = false;
static long g_por_seen = 0, g_retained = 0, g_por_other = 0;   // FREQ2 right after power-on

static void cc1101PowerOff()
{
    detachInterrupt(digitalPinToInterrupt(kGdo0));
    SPI.end();
    for (int pin : { kSck, kMiso, kMosi, kCsn, kGdo0 }) pinMode(pin, INPUT_PULLDOWN);
    digitalWrite(kVcc, LOW);
}

static void cc1101PowerOn()
{
    digitalWrite(kVcc, HIGH);
    delay(2);   // supply ramp; CHIP_RDYn is still checked by every access
    pinMode(kCsn, OUTPUT);
    digitalWrite(kCsn, HIGH);
    SPI.begin(kSck, kMiso, kMosi, -1);
    pinMode(kGdo0, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(kGdo0), onGdo0, CHANGE);
}

// One cold start: off for off_ms, on, then the product bring-up path.
static bool powerCycleOnce(uint32_t off_ms, bool do_reset, char *why, size_t why_len)
{
    cc1101PowerOff();
    delay(off_ms);
    cc1101PowerOn();
    bool edge = false, cal = false;
    cc1101_apply_report_t r = { 0, 0, 0, -1 };
    cc1101_err_t err = cc1101_probe(&g_bus, nullptr, nullptr);
    uint8_t f2 = 0;
    if (err == CC1101_OK && cc1101_read_reg(&g_bus, CC1101_FREQ2, &f2) == CC1101_OK) {
        if (f2 == CC1101_POR_FREQ2) ++g_por_seen;
        else if (f2 == g_profile.reg[CC1101_FREQ2]) ++g_retained;
        else ++g_por_other;
    }
    if (err == CC1101_OK && do_reset) err = cc1101_reset(&g_bus, &edge);
    if (err == CC1101_OK) err = cc1101_apply_profile(&g_bus, &g_profile, &r);
    if (err == CC1101_OK) err = cc1101_rx_start(&g_bus, &cal);
    if (err != CC1101_OK)
        snprintf(why, why_len, "err=%s,failed=%u,first_failed=%d", cc1101_err_name(err), r.failed,
                 r.first_failed);
    return err == CC1101_OK && cal;
}

static void powerCycles(long n, uint32_t off_ms, bool do_reset)
{
    if (!g_vcc_gpio) { Serial.println("E,vcc_gpio_off: send 'vccgpio' first"); return; }
    long fail = 0;
    char first[96] = "";
    const uint32_t t0 = millis();
    g_por_seen = g_retained = g_por_other = 0;
    for (long i = 0; i < n; i++) {
        char why[96] = "";
        if (!powerCycleOnce(off_ms, do_reset, why, sizeof why)) {
            if (!fail) snprintf(first, sizeof first, "i=%ld,%s", i, why);
            ++fail;
        }
        delay(1);
    }
    Serial.printf("P,n=%ld,off_ms=%lu,reset=%d,fail=%ld,ms=%lu,por=%ld,retained=%ld,other=%ld,first_fail=%s\n",
                  n, (unsigned long)off_ms, do_reset, fail, (unsigned long)(millis() - t0),
                  g_por_seen, g_retained, g_por_other, first[0] ? first : "none");
}

static void handleCommand()
{
    static char buf[32];
    static size_t n = 0;
    while (Serial.available()) {
        const char c = Serial.read();
        if (c != '\n' && c != '\r') {
            if (n + 1 < sizeof buf) buf[n++] = c;
            continue;
        }
        buf[n] = '\0';
        if (!strcmp(buf, "i")) printInfo();
        else if (!strcmp(buf, "d")) {
            Serial.print("D");
            for (uint8_t a = 0; a < CC1101_CFG_COUNT; a++) {
                uint8_t v = 0;
                cc1101_read_reg(&g_bus, a, &v);
                Serial.printf(",%02x=%02x", a, v);
            }
            Serial.println();
        }
        else if (!strncmp(buf, "gap ", 4)) { g_gap_us = (uint32_t)atol(buf + 4); g_spi_quiet = false; }
        else if (!strcmp(buf, "loud")) g_spi_quiet = false;
        else if (!strcmp(buf, "quiet")) g_spi_quiet = true;
        else if (!strncmp(buf, "cycle ", 6)) stressCycles(true, atol(buf + 6));
        else if (!strncmp(buf, "rxcyc ", 6)) stressCycles(false, atol(buf + 6));
        else if (!strcmp(buf, "vccgpio")) {
            // Opt in only once VCC is actually wired to D4.
            pinMode(kVcc, OUTPUT);
            gpio_set_drive_capability((gpio_num_t)kVcc, GPIO_DRIVE_CAP_3);
            digitalWrite(kVcc, HIGH);
            g_vcc_gpio = true;
            Serial.println("I,vcc_gpio=1");
        }
        else if (!strncmp(buf, "pc ", 3) || !strncmp(buf, "pcr ", 4)) {
            // pc N OFF_MS: cold starts without SRES (POR only); pcr: with SRES.
            const bool with_reset = buf[2] == 'r';
            char *rest = buf + (with_reset ? 4 : 3);
            const long n = strtol(rest, &rest, 10);
            powerCycles(n, (uint32_t)strtoul(rest, nullptr, 10), with_reset);
            printInfo();
        }
        else if (!strcmp(buf, "up")) { bringUp(false); printInfo(); }
        else if (!strcmp(buf, "reset")) { bringUp(true); printInfo(); }
        n = 0;
    }
}

// Silent-reset/brown-out watchdog: verify the live registers against the profile.
static void checkDrift()
{
    static uint32_t last = 0;
    if (millis() - last < kVerifyMs) return;
    last = millis();
    int16_t at = -1;
    uint8_t marc = 0;
    const cc1101_err_t v = cc1101_verify_profile(&g_bus, &g_profile, &at);
    const cc1101_err_t m = cc1101_read_status(&g_bus, CC1101_MARCSTATE, &marc);
    if (v == CC1101_OK && m == CC1101_OK && (marc & 0x1F) == CC1101_MARC_RX) return;
    ++g_drift_count;
    Serial.printf("E,drift,count=%lu,verify=%s,first_mismatch=%d,marc=%s/%02x\n",
                  (unsigned long)g_drift_count, cc1101_err_name(v), at, cc1101_err_name(m), marc);
    bringUp(false);
    printInfo();
}

void setup()
{
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);
    delay(500);
    Serial.println("I,boot,driver=oscilla");

    pinMode(kCsn, OUTPUT);
    digitalWrite(kCsn, HIGH);
    SPI.begin(kSck, kMiso, kMosi, -1);
    g_profile = cc1101_profile_ook_433_async;

    bringUp(true);
    printInfo();
    pinMode(kGdo0, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(kGdo0), onGdo0, CHANGE);
    Serial.println("H,t_ms,rssi_max,rssi_mean,gdo0_edges");
}

void loop()
{
    handleCommand();
    if (!g_spi_quiet) checkDrift();
    const uint32_t start = millis();
    int peak = -200;
    long sum = 0;
    uint32_t count = 0;
    while (millis() - start < kWindowMs) {
        if (g_spi_quiet) { delay(1); continue; }
        if (g_gap_us) delayMicroseconds(g_gap_us);
        int16_t dbm = 0;
        if (cc1101_rx_rssi(&g_bus, &dbm) != CC1101_OK) continue;
        if (dbm > peak) peak = dbm;
        sum += dbm;
        ++count;
    }
    noInterrupts();
    const uint32_t edges = g_edges;
    g_edges = 0;
    interrupts();
    Serial.printf("R,%lu,%d,%ld,%lu\n", (unsigned long)millis(), peak,
                  count ? sum / (long)count : 0L, (unsigned long)edges);
}
