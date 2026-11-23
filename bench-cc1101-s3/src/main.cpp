// bench-cc1101-s3 — receive-only CC1101 RSSI reference (D-15 requalification).
// NOT Oscilla. Never calls SetTx/SendData/setPA. Output protocol: README.md.
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SPI.h>

// XIAO ESP32-S3; same XIAO labels as the archived C5 harness (D8/D9/D10, D3 CSn).
constexpr byte kSck = 7;    // D8
constexpr byte kMiso = 8;   // D9
constexpr byte kMosi = 9;   // D10
constexpr byte kCsn = 4;    // D3
constexpr byte kGdo0 = 2;   // D1

constexpr uint32_t kWindowMs = 20;
constexpr uint32_t kSpiHz = 1000000;   // SmartRC default; lower to test signal integrity
constexpr float kDefaultMhz = 433.92f;
constexpr float kDefaultRxBwKhz = 650.0f;

volatile uint32_t g_edges = 0;
float g_mhz = kDefaultMhz;
// 0 = SmartRC profile; 1 = archived C5 driver profile (cc1101 branch, da4c516),
// no calibration; 2 = same plus one SCAL strobe before SRX.
int g_profile = 0;
bool g_quiet = false;   // 'quiet': no RSSI polling, no auto-reconfigure (brown-out isolation)
uint32_t g_fifo_bytes = 0, g_overflows = 0;

void IRAM_ATTR onGdo0() { g_edges++; }

void printConfig()
{
    const byte partnum = ELECHOUSE_cc1101.SpiReadStatus(0x30);
    const byte version = ELECHOUSE_cc1101.SpiReadStatus(0x31);
    const uint32_t freq = (uint32_t(ELECHOUSE_cc1101.SpiReadReg(0x0D)) << 16) |
                          (uint32_t(ELECHOUSE_cc1101.SpiReadReg(0x0E)) << 8) |
                          ELECHOUSE_cc1101.SpiReadReg(0x0F);
    // f = FREQ * f_xosc / 2^16; shown for both common crystals.
    Serial.printf("I,partnum=0x%02x,version=0x%02x,freq_reg=0x%06lx,mhz_at_26=%.4f,mhz_at_27=%.4f,"
                  "set_mhz=%.3f,mode=%u\n",
                  partnum, version, (unsigned long)freq, freq * 26.0 / 65536.0,
                  freq * 27.0 / 65536.0, g_mhz, ELECHOUSE_cc1101.getMode());
}

// SmartRC's working profile (Init + async OOK, 433.92 MHz, 650 kHz) as read back
// from the chip on 2026-11-23. Re-calling SmartRC Init() at runtime wedges the
// chip, so runtime restores write this table instead. MCSM0=0x18 recalibrates on SRX.
const byte kSmartRcRegs[0x2F] = {
    0x0D, 0x2E, 0x0D, 0x07, 0xD3, 0x91, 0x00, 0x04,
    0x32, 0x00, 0x00, 0x0F, 0x23, 0x10, 0xB0, 0x71,
    0x17, 0x93, 0x32, 0x02, 0xF8, 0x47, 0x07, 0x30,
    0x18, 0x16, 0x1C, 0xC7, 0x00, 0xB2, 0x87, 0x6B,
    0xF8, 0x56, 0x11, 0xEF, 0x2B, 0x17, 0x1F, 0x41,
    0x00, 0x59, 0x7F, 0x3C, 0x81, 0x35, 0x09,
};

byte g_por[0x2F];
bool g_isr = false;

void gdo0Isr(bool on)
{
    if (on && !g_isr) attachInterrupt(digitalPinToInterrupt(kGdo0), onGdo0, CHANGE);
    if (!on && g_isr) detachInterrupt(digitalPinToInterrupt(kGdo0));
    g_isr = on;
}

void capturePor()
{
    byte again[0x2F];
    for (byte a = 0; a < 0x2F; ++a) g_por[a] = ELECHOUSE_cc1101.SpiReadReg(a);
    for (byte a = 0; a < 0x2F; ++a) again[a] = ELECHOUSE_cc1101.SpiReadReg(a);
    Serial.print("P");
    for (byte a = 0; a < 0x2F; ++a) Serial.printf(",%02x=%02x", a, g_por[a]);
    int mismatch = 0;
    for (byte a = 0; a < 0x2F; ++a) mismatch += g_por[a] != again[a];
    Serial.printf(",mismatch=%d\n", mismatch);
}

// Write-and-verify with bounded retries; returns attempts used (0 = never stuck).
int writeVerified(byte addr, byte value)
{
    for (int k = 1; k <= 50; ++k) {
        ELECHOUSE_cc1101.SpiWriteReg(addr, value);
        if (ELECHOUSE_cc1101.SpiReadReg(addr) == value) return k;
    }
    return 0;
}

// GDO pins first: the POR XOSC/192 clock on GDO0 is the suspected SPI aggressor.
void writeAll(const byte *regs)
{
    gdo0Isr(false);
    ELECHOUSE_cc1101.setSidle();
    const int g0 = writeVerified(0x02, 0x2E), g2 = writeVerified(0x00, 0x2E);
    int retried = 0, failed = 0;
    for (byte a = 0; a < 0x2F; ++a) {
        const int k = writeVerified(a, regs[a]);
        failed += k == 0;
        retried += k > 1;
    }
    Serial.printf("I,writeall,gdo0_tries=%d,gdo2_tries=%d,retried=%d,failed=%d\n", g0, g2, retried, failed);
}

void applySmartTable()
{
    writeAll(kSmartRcRegs);
    ELECHOUSE_cc1101.SpiStrobe(0x3A);   // SFRX
    ELECHOUSE_cc1101.SpiStrobe(0x34);   // SRX (autocal)
    g_profile = 0;
    gdo0Isr(true);
}

// Exactly what the archived C5 cc1101_radio_rx_start() wrote (its SRES replaced
// by rewriting the captured POR table): FREQ for
// 433.92 MHz at 26 MHz, MDMCFG4/3 from cc1101_regs.c (25985 Bd, 203125 Hz),
// MDMCFG2 OOK + carrier-sense, AGCCTRL1 0x58, PKTCTRL0 0x02, MCSM1 0x0C,
// IOCFG1 0x2E, SFRX, SRX. Everything else stays at POR (MCSM0 FS_AUTOCAL=0).
void applyC5Profile(bool calibrate)
{
    writeAll(g_por);   // POR state without SRES; IOCFG0 POR puts a clock on GDO0 (ISR off)
    const uint32_t f = (uint32_t)((433920000ULL << 16) / 26000000ULL);
    ELECHOUSE_cc1101.SpiWriteReg(0x0D, byte(f >> 16));
    ELECHOUSE_cc1101.SpiWriteReg(0x0E, byte(f >> 8));
    ELECHOUSE_cc1101.SpiWriteReg(0x0F, byte(f));
    ELECHOUSE_cc1101.SpiWriteReg(0x10, 0x8A);
    ELECHOUSE_cc1101.SpiWriteReg(0x11, 0x06);
    ELECHOUSE_cc1101.SpiWriteReg(0x12, 0x34);
    ELECHOUSE_cc1101.SpiWriteReg(0x1C, 0x58);
    ELECHOUSE_cc1101.SpiWriteReg(0x08, 0x02);
    ELECHOUSE_cc1101.SpiWriteReg(0x17, 0x0C);
    ELECHOUSE_cc1101.SpiWriteReg(0x01, 0x2E);
    ELECHOUSE_cc1101.SpiStrobe(0x3A);                // SFRX
    if (calibrate) {
        ELECHOUSE_cc1101.SpiStrobe(0x33);            // SCAL from IDLE
        delay(2);
    }
    ELECHOUSE_cc1101.SpiStrobe(0x34);                // SRX
    g_profile = calibrate ? 2 : 1;
    g_fifo_bytes = g_overflows = 0;
    Serial.printf("I,profile=%d,fscal1=%02x,marcstate=%02x\n", g_profile,
                  ELECHOUSE_cc1101.SpiReadReg(0x25), ELECHOUSE_cc1101.SpiReadStatus(0x35) & 0x1F);
}

// The C5 poll loop: drain the FIFO, and on overflow SIDLE/SFRX/SRX (no recalibration).
void serviceC5Fifo()
{
    const byte rx = ELECHOUSE_cc1101.SpiReadStatus(0x3B);
    if (rx & 0x80) {
        ++g_overflows;
        ELECHOUSE_cc1101.SpiStrobe(0x36);
        ELECHOUSE_cc1101.SpiStrobe(0x3A);
        ELECHOUSE_cc1101.SpiStrobe(0x34);
        return;
    }
    byte n = rx & 0x7F;
    if (n > 64) n = 64;   // FIFO is 64 bytes; a garbled RXBYTES must not overrun buf
    if (n > 1) {
        byte buf[64];
        ELECHOUSE_cc1101.SpiReadBurstReg(0x3F, buf, n - 1);
        g_fifo_bytes += n - 1;
    }
}

void dumpRegisters()
{
    Serial.print("D");
    for (byte a = 0x00; a <= 0x2E; ++a) Serial.printf(",%02x=%02x", a, ELECHOUSE_cc1101.SpiReadReg(a));
    Serial.println();
}

void tune(float mhz)
{
    g_mhz = mhz;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.SetRx();
    printConfig();
}

void handleCommand()
{
    static char buf[24];
    static size_t n = 0;
    while (Serial.available()) {
        const char c = Serial.read();
        if (c != '\n' && c != '\r') {
            if (n + 1 < sizeof buf) buf[n++] = c;
            continue;
        }
        buf[n] = '\0';
        if (n == 1 && buf[0] == 'd') dumpRegisters();
        else if (n == 1 && buf[0] == 'i') printConfig();
        else if (n == 1 && buf[0] == 'w') {
            // All 256 CHANNR (0x0A) write/readback values; MARCSTATE 0x0D = RX.
            const byte saved = ELECHOUSE_cc1101.SpiReadReg(0x0A);
            int bad = 0, shown = 0;
            byte or_diff = 0;
            Serial.printf("W,marcstate=%02x", ELECHOUSE_cc1101.SpiReadStatus(0x35) & 0x1F);
            for (int v = 0; v < 256; ++v) {
                ELECHOUSE_cc1101.SpiWriteReg(0x0A, byte(v));
                const byte r = ELECHOUSE_cc1101.SpiReadReg(0x0A);
                if (r != v) {
                    ++bad;
                    or_diff |= byte(r ^ v);
                    if (shown++ < 12) Serial.printf(",%02x->%02x", v, r);
                }
            }
            ELECHOUSE_cc1101.SpiWriteReg(0x0A, saved);
            Serial.printf(",bad=%d,diff_bits=%02x\n", bad, or_diff);
        }
        else if (n == 1 && buf[0] == 'v') {
            // SPI integrity: stable ID reads, and CHANNR (0x0A, harmless) write/readback.
            Serial.print("V,id");
            for (int k = 0; k < 10; ++k)
                Serial.printf(",%02x/%02x", ELECHOUSE_cc1101.SpiReadStatus(0x30),
                              ELECHOUSE_cc1101.SpiReadStatus(0x31));
            Serial.print(",chanr");
            const byte saved = ELECHOUSE_cc1101.SpiReadReg(0x0A);
            for (byte v : {byte(0x00), byte(0x55), byte(0xAA)}) {
                ELECHOUSE_cc1101.SpiWriteReg(0x0A, v);
                Serial.printf(",%02x->%02x", v, ELECHOUSE_cc1101.SpiReadReg(0x0A));
            }
            ELECHOUSE_cc1101.SpiWriteReg(0x0A, saved);
            Serial.println();
        }
        else if (n == 2 && buf[0] == 'x' && (buf[1] == 'r' || buf[1] == 'i')) {
            // Classify CHANNR write faults: 'xr' in RX, 'xi' in IDLE.
            // stale1 = 1st read wrong, 2nd right (read timing); dropped = both reads
            // wrong; other = any other mismatch. Collateral = other registers that
            // changed across the sweep (a misdirected write lands there).
            byte before[0x2F], after[0x2F];
            if (buf[1] == 'i') ELECHOUSE_cc1101.setSidle();
            for (byte a = 0; a < 0x2F; ++a) before[a] = ELECHOUSE_cc1101.SpiReadReg(a);
            const byte saved = before[0x0A];
            int stale1 = 0, dropped = 0, other = 0, shown = 0;
            Serial.printf("X,state=%c,marcstate=%02x", buf[1],
                          ELECHOUSE_cc1101.SpiReadStatus(0x35) & 0x1F);
            for (int v = 0; v < 256; ++v) {
                ELECHOUSE_cc1101.SpiWriteReg(0x0A, byte(v));
                const byte r1 = ELECHOUSE_cc1101.SpiReadReg(0x0A);
                const byte r2 = ELECHOUSE_cc1101.SpiReadReg(0x0A);
                if (r1 == v && r2 == v) continue;
                if (r1 != v && r2 == v) ++stale1;
                else if (r1 == r2) ++dropped;
                else ++other;
                if (shown++ < 8) Serial.printf(",%02x:%02x/%02x", v, r1, r2);
            }
            ELECHOUSE_cc1101.SpiWriteReg(0x0A, saved);
            for (byte a = 0; a < 0x2F; ++a) after[a] = ELECHOUSE_cc1101.SpiReadReg(a);
            Serial.printf(",stale1=%d,dropped=%d,other=%d,collateral", stale1, dropped, other);
            for (byte a = 0; a < 0x2F; ++a)
                if (a != 0x0A && before[a] != after[a]) Serial.printf(",%02x:%02x->%02x", a, before[a], after[a]);
            Serial.println();
            if (buf[1] == 'i') ELECHOUSE_cc1101.SetRx();
        }
        else if (!strcmp(buf, "pins")) {
            // Line-level check with SPI released. An unpowered CC1101 clamps its
            // pins toward 0 V through ESD diodes, so pulled-up lines read low.
            gdo0Isr(false);
            SPI.end();
            pinMode(kCsn, INPUT);                       // external 10k pull-up to 3V3 only
            delay(2);
            const int csn_float = digitalRead(kCsn);
            pinMode(kCsn, OUTPUT); digitalWrite(kCsn, HIGH);
            pinMode(kMiso, INPUT_PULLUP); delay(2);
            const int so_pu = digitalRead(kMiso);
            pinMode(kMiso, INPUT_PULLDOWN); delay(2);
            const int so_pd = digitalRead(kMiso);
            digitalWrite(kCsn, LOW); delayMicroseconds(200);
            pinMode(kMiso, INPUT_PULLUP); delayMicroseconds(200);
            const int so_cs_low = digitalRead(kMiso);
            digitalWrite(kCsn, HIGH);
            int g_hi = 0;
            pinMode(kGdo0, INPUT_PULLDOWN);
            for (int k = 0; k < 1000; ++k) { g_hi += digitalRead(kGdo0); delayMicroseconds(3); }
            pinMode(kGdo0, INPUT_PULLUP); delay(1);
            int g_hi_pu = 0;
            for (int k = 0; k < 1000; ++k) { g_hi_pu += digitalRead(kGdo0); delayMicroseconds(3); }
            pinMode(kGdo0, INPUT_PULLDOWN);
            pinMode(kMiso, INPUT_PULLUP);
            Serial.printf("L,csn_float=%d,so_cs_high_pu=%d,so_cs_high_pd=%d,so_cs_low_pu=%d,gdo0_high_of_1000_pd=%d,pu=%d\n",
                          csn_float, so_pu, so_pd, so_cs_low, g_hi, g_hi_pu);
        }
        else if (!strcmp(buf, "bb")) {
            // Software SPI (mode 0, ~10 kHz), no S3 SPI peripheral involved.
            gdo0Isr(false);
            SPI.end();
            pinMode(kSck, OUTPUT); digitalWrite(kSck, LOW);
            pinMode(kMosi, OUTPUT); pinMode(kMiso, INPUT_PULLUP);
            pinMode(kCsn, OUTPUT); digitalWrite(kCsn, HIGH);
            auto xfer = [](byte out) {
                byte in = 0;
                for (int b = 7; b >= 0; --b) {
                    digitalWrite(kMosi, (out >> b) & 1);
                    delayMicroseconds(50);
                    digitalWrite(kSck, HIGH);
                    in = (in << 1) | digitalRead(kMiso);
                    delayMicroseconds(50);
                    digitalWrite(kSck, LOW);
                }
                return in;
            };
            auto txn = [&](byte a, byte d, byte *status) {
                digitalWrite(kCsn, LOW);
                const uint32_t t = micros();
                while (digitalRead(kMiso) && micros() - t < 10000) {}
                const byte st = xfer(a);
                const byte v = xfer(d);
                digitalWrite(kCsn, HIGH);
                delayMicroseconds(100);
                if (status) *status = st;
                return v;
            };
            byte st = 0;
            const byte ver = txn(0x31 | 0xC0, 0, &st);
            const byte part = txn(0x30 | 0xC0, 0, nullptr);
            const byte marc = txn(0x35 | 0xC0, 0, nullptr);
            const byte f2 = txn(0x0D | 0x80, 0, nullptr);
            const byte io0 = txn(0x02 | 0x80, 0, nullptr);
            txn(0x0A, 0x5A, nullptr);
            const byte ch = txn(0x0A | 0x80, 0, nullptr);
            Serial.printf("B,status=%02x,version=%02x,partnum=%02x,marcstate=%02x,freq2=%02x,iocfg0=%02x,chanr_5a=%02x\n",
                          st, ver, part, marc, f2, io0, ch);
        }
        else if (!strcmp(buf, "quiet")) { g_quiet = true; Serial.println("I,quiet"); }
        else if (!strcmp(buf, "loud")) { g_quiet = false; Serial.println("I,loud"); }
        else if (!strcmp(buf, "sidle")) ELECHOUSE_cc1101.SpiStrobe(0x36);
        else if (!strcmp(buf, "srx")) ELECHOUSE_cc1101.SpiStrobe(0x34);
        else if (!strcmp(buf, "mark")) ELECHOUSE_cc1101.SpiWriteReg(0x0A, 0x5A);
        else if (!strcmp(buf, "peek")) {
            // Marker 0x5A in CHANNR survives unless the chip power-on resets.
            Serial.printf("K,%lu,version=%02x,chanr=%02x,marcstate=%02x\n", (unsigned long)millis(),
                          ELECHOUSE_cc1101.SpiReadStatus(0x31), ELECHOUSE_cc1101.SpiReadReg(0x0A),
                          ELECHOUSE_cc1101.SpiReadStatus(0x35) & 0x1F);
        }
        else if (!strcmp(buf, "por")) {
            Serial.print("P");
            for (byte a = 0; a < 0x2F; ++a) Serial.printf(",%02x=%02x", a, g_por[a]);
            Serial.println();
        }
        else if (!strcmp(buf, "c5")) applyC5Profile(false);
        else if (!strcmp(buf, "c5cal")) applyC5Profile(true);
        else if (!strcmp(buf, "smart")) {
            applySmartTable();
            Serial.printf("I,profile=0,fscal1=%02x,marcstate=%02x\n", ELECHOUSE_cc1101.SpiReadReg(0x25),
                          ELECHOUSE_cc1101.SpiReadStatus(0x35) & 0x1F);
        }
        else if (n > 1 && buf[0] == 'm') {
            const float mhz = atof(buf + 1);
            if (mhz >= 300.0f && mhz <= 928.0f) tune(mhz);
            else Serial.println("E,bad_mhz");
        }
        n = 0;
    }
}

void setup()
{
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    delay(2500);   // let the host attach before anything can wedge
    Serial.println("I,boot");
    // Diagnostic: slow SPI to separate wiring signal integrity from driver behaviour.
    SPI.begin(kSck, kMiso, kMosi, kCsn);
    SPI.setFrequency(kSpiHz);
    SPI.end();   // SmartRC re-begins per access; the divider survives end()
    Serial.printf("I,spi_hz=%lu\n", (unsigned long)kSpiHz);
    // SmartRC spins forever on MISO high; check chip-ready (SO low after CSn low) with a bound first.
    pinMode(kCsn, OUTPUT);
    pinMode(kMiso, INPUT_PULLUP);
    for (;;) {
        digitalWrite(kCsn, LOW);
        const uint32_t t = micros();
        bool ready = false;
        while (micros() - t < 50000) {
            if (digitalRead(kMiso) == LOW) { ready = true; break; }
        }
        digitalWrite(kCsn, HIGH);
        if (ready) break;
        Serial.println("E,miso_stuck_high: CC1101 not answering (check 3V3, GND, CSn->D3, SO->D9)");
        delay(1000);
    }
    Serial.println("I,chip_ready");

    ELECHOUSE_cc1101.setSpiPin(kSck, kMiso, kMosi, kCsn);
    ELECHOUSE_cc1101.setGDO0(kGdo0);
    if (!ELECHOUSE_cc1101.getCC1101()) {
        Serial.println("E,no_cc1101 (VERSION read 0): check wiring/CSn/3V3");
    }
    // No SRES anywhere: on this module SO never falls after SRES and the chip
    // then ignores writes until power-cycled (WORKLOG 2026-11-23). Capture the
    // power-on registers (twice, to catch read glitches), then load SmartRC's table.
    capturePor();
    applySmartTable();

    gdo0Isr(false);
    pinMode(kGdo0, INPUT_PULLDOWN);   // unconnected GDO0 reads 0 edges, not noise
    gdo0Isr(true);
    printConfig();
    dumpRegisters();
    Serial.println("H,t_ms,rssi_max,rssi_mean,gdo0_edges");
}

// A power-on reset restores FREQ to 0x1EC4EC (800 MHz) and drops to IDLE.
void checkForChipReset()
{
    static uint32_t last_ms = 0, resets = 0;
    if (millis() - last_ms < 1000) return;
    last_ms = millis();
    const byte f2 = ELECHOUSE_cc1101.SpiReadReg(0x0D);
    const byte marc = ELECHOUSE_cc1101.SpiReadStatus(0x35) & 0x1F;
    if (f2 == 0x1E || marc != 0x0D) {
        ++resets;
        Serial.printf("E,chip_reset_or_not_rx,count=%lu,freq2=%02x,marcstate=%02x\n",
                      (unsigned long)resets, f2, marc);
        applySmartTable();   // full reapply so each brown-out is counted separately
    }
}

void loop()
{
    handleCommand();
    if (g_quiet) { delay(5); return; }
    if (g_profile == 0) checkForChipReset();
    const uint32_t start = millis();
    int peak = -200;
    long sum = 0;
    uint32_t count = 0;
    while (millis() - start < kWindowMs) {
        const int r = ELECHOUSE_cc1101.getRssi();
        if (r > peak) peak = r;
        sum += r;
        ++count;
        if (g_profile != 0 && count % 16 == 0) serviceC5Fifo();
    }
    noInterrupts();
    const uint32_t edges = g_edges;
    g_edges = 0;
    interrupts();
    // In C5 profiles the 4th column is FIFO bytes drained (carrier-sense opened), not GDO0 edges.
    const uint32_t col4 = g_profile ? g_fifo_bytes : edges;
    g_fifo_bytes = 0;
    Serial.printf("R,%lu,%d,%ld,%lu\n", (unsigned long)millis(), peak,
                  count ? sum / (long)count : 0L, (unsigned long)col4);
}
