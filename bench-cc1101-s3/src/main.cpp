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
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setCCMode(0);        // async serial: demodulated OOK on GDO0
    ELECHOUSE_cc1101.setModulation(2);    // ASK/OOK
    ELECHOUSE_cc1101.setRxBW(kDefaultRxBwKhz);
    ELECHOUSE_cc1101.setMHZ(kDefaultMhz);
    ELECHOUSE_cc1101.SetRx();

    pinMode(kGdo0, INPUT_PULLDOWN);   // unconnected GDO0 reads 0 edges, not noise
    attachInterrupt(digitalPinToInterrupt(kGdo0), onGdo0, CHANGE);
    printConfig();
    dumpRegisters();
    Serial.println("H,t_ms,rssi_max,rssi_mean,gdo0_edges");
}

void loop()
{
    handleCommand();
    const uint32_t start = millis();
    int peak = -200;
    long sum = 0;
    uint32_t count = 0;
    while (millis() - start < kWindowMs) {
        const int r = ELECHOUSE_cc1101.getRssi();
        if (r > peak) peak = r;
        sum += r;
        ++count;
    }
    noInterrupts();
    const uint32_t edges = g_edges;
    g_edges = 0;
    interrupts();
    Serial.printf("R,%lu,%d,%ld,%lu\n", (unsigned long)millis(), peak,
                  count ? sum / (long)count : 0L, (unsigned long)edges);
}
