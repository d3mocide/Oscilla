/*
 * meter_test.cpp — host tests for the power meter's pure logic. Adversarial
 * first (AGENTS.md §6): meter output must never split a probe line, however
 * the probe's bytes are chunked.
 *
 * SPDX-License-Identifier: MIT
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "display_text.h"
#include "ina_decode.h"
#include "meter_cmd.h"
#include "meter_mux.h"
#include "meter_window.h"
#include "probe_power.h"

using namespace meter;

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

struct StrSink : Sink {
    std::string out;
    void write(const uint8_t *p, size_t n) override { out.append(reinterpret_cast<const char *>(p), n); }
};

struct Cmds : CommandSink {
    std::vector<std::string> lines;
    std::vector<bool> overflow;
    void command(const char *line, bool of) override
    {
        lines.emplace_back(line);
        overflow.push_back(of);
    }
};

void fwd(Downlink &d, const std::string &s, int64_t t, Sink &h)
{
    d.forward(reinterpret_cast<const uint8_t *>(s.data()), s.size(), t, h);
}

void up(Uplink &u, const std::string &s, Sink *probe, CommandSink &c)
{
    u.feed(reinterpret_cast<const uint8_t *>(s.data()), s.size(), probe, c);
}

/* Every line of `out` is either a whole probe line or a whole meter line. */
bool linesIntact(const std::string &out, const std::vector<std::string> &probe_lines)
{
    size_t pos = 0, next_probe = 0;
    while (pos < out.size()) {
        size_t nl = out.find('\n', pos);
        if (nl == std::string::npos) return false;
        std::string line = out.substr(pos, nl - pos);
        if (line.rfind("#PWR", 0) != 0) {
            if (next_probe >= probe_lines.size() || line != probe_lines[next_probe]) return false;
            next_probe++;
        }
        pos = nl + 1;
    }
    return next_probe == probe_lines.size();
}

void testDownlink()
{
    const std::vector<std::string> probe = {
        "[SCAN] BEGIN n=2", "[SCAN] 1 AA:BB \"x\" -40", "[EVT] kind=sniff pkts=3", "[SCAN] END"};
    std::string wire;
    for (auto &l : probe) wire += l + "\n";

    {   /* One byte at a time, a meter line queued before every byte. */
        Downlink d;
        StrSink h;
        for (size_t i = 0; i < wire.size(); i++) {
            d.queue("#PWR win n=1");
            fwd(d, wire.substr(i, 1), static_cast<int64_t>(i), h);
            d.service(static_cast<int64_t>(i), h);
        }
        check(linesIntact(h.out, probe), "byte-at-a-time: meter lines never split a probe line");
        check(d.cuts() == 0, "byte-at-a-time: nothing was cut");
    }
    {   /* Mid-line: service() must wait, not insert. */
        Downlink d;
        StrSink h;
        fwd(d, "[STATUS] owner=wi", 0, h);
        d.queue("#PWR win n=1");
        d.service(Downlink::kStallUs - 1, h);
        check(h.out == "[STATUS] owner=wi", "mid-line meter output waits for the boundary");
        fwd(d, "fi END\n", 10, h);
        check(h.out == "[STATUS] owner=wifi END\n#PWR win n=1\n", "it goes out right after the newline");
    }
    {   /* A probe that dies mid-line must not hold meter output forever. */
        Downlink d;
        StrSink h;
        fwd(d, "ESP-ROM:esp32c5-eco", 0, h);
        d.queue("#PWR win n=1");
        d.service(Downlink::kStallUs, h);
        check(h.out == "ESP-ROM:esp32c5-eco\n#PWR win n=1\n", "a stalled line is cut, then meter output flows");
        check(d.cuts() == 1, "the cut is counted");
    }
    {
        Downlink d;
        StrSink h;
        fwd(d, "[HELLO] proto=1", 0, h);
        d.probeGone(h);
        check(h.out == "[HELLO] proto=1\n" && d.cuts() == 1, "probe power loss terminates its partial line");
        fwd(d, "x\n", 1, h);
        d.probeGone(h);
        check(d.cuts() == 1, "no cut when already at a boundary");
    }
    {
        Downlink d;
        std::string big(Downlink::kPendingCap - 1, 'a');
        check(d.queue(big.c_str()), "a line exactly filling the buffer queues");
        check(!d.queue("b"), "overflow is refused, not truncated");
    }
}

void testUplink()
{
    {
        Uplink u;
        StrSink p;
        Cmds c;
        up(u, "stop\n#mark row1\nstatus\n", &p, c);
        check(p.out == "stop\nstatus\n", "meter commands never reach the probe");
        check(c.lines.size() == 1 && c.lines[0] == "mark row1", "the meter command is delivered");
    }
    {   /* '#' only counts at a line start. */
        Uplink u;
        StrSink p;
        Cmds c;
        up(u, "lora_config 9#15\n", &p, c);
        check(p.out == "lora_config 9#15\n" && c.lines.empty(), "mid-line '#' is probe data");
    }
    {   /* Byte-at-a-time with CRLF. */
        Uplink u;
        StrSink p;
        Cmds c;
        std::string s = "ping\r\n#info\r\nping\r\n";
        for (char ch : s) up(u, std::string(1, ch), &p, c);
        check(p.out == "ping\r\nping\r\n", "byte-at-a-time pass-through is exact");
        check(c.lines.size() == 1 && c.lines[0] == "info", "CR is stripped from meter commands");
    }
    {
        Uplink u;
        StrSink p;
        Cmds c;
        up(u, "#" + std::string(Uplink::kCmdCap + 10, 'x') + "\nping\n", &p, c);
        check(c.lines.size() == 1 && c.overflow[0], "an overlong command is flagged, not run");
        check(p.out == "ping\n", "and the stream recovers after it");
    }
    {
        Uplink u;
        Cmds c;
        up(u, "ping\n#info\n", nullptr, c);
        check(u.dropped() == 5 && c.lines.size() == 1, "probe down: data dropped and counted, commands still work");
    }
}

void testCommands()
{
    check(parseCommand("stream on", false).kind == CmdKind::StreamOn, "stream on");
    check(parseCommand("window 50", false).value == 50, "window 50");
    check(parseCommand("window 5", false).kind == CmdKind::Invalid, "window below range rejected");
    check(parseCommand("shunt 20000", false).value == 20000, "shunt in µΩ");
    check(parseCommand("shunt 99999999999999999999", false).kind == CmdKind::Invalid, "overflowing number rejected");
    check(parseCommand("shunt 20k", false).kind == CmdKind::Invalid, "trailing junk rejected");
    check(parseCommand("sag 0", false).kind == CmdKind::Sag, "sag 0 disables");
    check(std::strcmp(parseCommand("mark row6_both", false).label, "row6_both") == 0, "mark label");
    check(parseCommand("mark a\"b", false).kind == CmdKind::Invalid, "quote in a label rejected");
    check(parseCommand("mark", false).kind == CmdKind::Invalid, "empty label rejected");
    std::string long_mark = "mark " + std::string(33, 'a');
    check(parseCommand(long_mark.c_str(), false).kind == CmdKind::Invalid, "overlong label rejected");
    check(parseCommand("stream on off", false).kind == CmdKind::Invalid, "extra argument rejected");
    check(parseCommand("info", true).kind == CmdKind::Invalid, "overflow flag wins");
}

void testWindow()
{
    const ShuntScale s226{kIna226ShuntLsbPv, kIna226FullScale};
    const ShuntScale s228{kIna228ShuntLsbPv, kIna228FullScale};

    /* 20 mΩ: 1 LSB = 2.5 µV = 125 µA. */
    check(rawToMicroamps(1, s226.lsb_pv, 20000) == 125, "INA226: 1 LSB at 20 mΩ is 125 µA");
    check(rawToMicroamps(-8, s226.lsb_pv, 20000) == -1000, "negative current keeps its sign");
    /* 50 mΩ: 1 LSB = 312.5 nV = 6.25 µA; 16 LSB = 100 µA exactly. */
    check(rawToMicroamps(16, s228.lsb_pv, 50000) == 100, "INA228: 16 LSB at 50 mΩ is 100 µA");
    check(rawToMicroamps(static_cast<int64_t>(kIna228FullScale) * 10000, s228.lsb_pv, 50000)
              == static_cast<int64_t>(kIna228FullScale) * 62500,
          "INA228: a long window's sum does not overflow");

    Window w;
    windowReset(w, 1000);
    windowAddShunt(w, 800, s226);   /* 100 mA */
    windowAddShunt(w, 2400, s226);  /* 300 mA */
    windowAddShunt(w, INT16_MAX, s226);
    windowAddBus(w, 3300);
    windowAddBus(w, 3120);
    windowAddSag(w, true);
    windowAddSag(w, false);
    check(w.clip == 1, "full-scale sample counted as clipped");
    char line[320];
    WindowFlags f{0, 0, 0, 0};
    int n = formatWindow(line, sizeof line, w, 51000, 20000, s226.lsb_pv, f);
    check(n > 0 && std::strstr(line, " i_min_ua=100000 ") && std::strstr(line, " vbus_min_mv=3120 ") &&
              std::strstr(line, " sag=1 "), "window line carries min, bus minimum and sag");
    check(formatWindow(line, 20, w, 51000, 20000, s226.lsb_pv, f) == -1, "a too-small buffer is refused");

    Window w8;
    windowReset(w8, 0);
    windowAddShunt(w8, INT16_MAX, s228);
    check(w8.clip == 0, "INA226 full scale is not clipping on an INA228");
    windowAddShunt(w8, -kIna228FullScale - 1, s228);
    check(w8.clip == 1, "INA228 negative full scale counts as clipped");

    windowReset(w, 51000);
    check(w.n == 0 && w.vbus_min_mv == 3120, "reset keeps only the latest bus reading");
    check(!w.sag, "reset clears sag");
}

void testDecode()
{
    check(ina226Shunt(0xFFFF) == -1, "INA226 shunt is two's complement");
    check(ina226BusMv(2640) == 3300, "INA226 bus: 2640 LSB is 3.3 V");
    check(ina226AlertLimit(3150) == 2520, "INA226 alert limit for 3.15 V");

    /* 24-bit register, value in bits 23:4. */
    check(ina228Raw20(0x000010) == 1, "INA228: bits 3:0 are not part of the value");
    check(ina228Raw20(0xFFFFF0) == -1, "INA228: sign extends from bit 23");
    check(ina228Raw20(0x7FFFF0) == kIna228FullScale, "INA228: positive full scale");
    check(ina228Raw20(0x800000) == -kIna228FullScale - 1, "INA228: negative full scale");
    check(ina228Raw20(0xFFFFFF) == -1, "INA228: reserved low bits ignored when set");
    check(ina228BusMv(16896) == 3300, "INA228 bus: 16896 LSB is 3.3 V");
    check(ina228Buvl(3150) == 1008, "INA228 BUVL for 3.15 V");
}

void testDisplay()
{
    char b[16];
    formatMa(b, sizeof b, 123456);
    check(std::strcmp(b, "123.4") == 0, "display: >= 100 mA shows one decimal");
    formatMa(b, sizeof b, 5678);
    check(std::strcmp(b, "5.67") == 0, "display: < 100 mA shows two decimals");
    formatMa(b, sizeof b, -250);
    check(std::strcmp(b, "-0.25") == 0, "display: small negative keeps its sign");
    formatMa(b, sizeof b, 1640000);
    check(std::strcmp(b, "1640.0") == 0, "display: full scale at R050 fits");

    DisplaySnapshot s;
    DisplayLines l;
    composeLines(s, l);
    check(std::strcmp(l.big, "NO SENSOR") == 0, "display: no sensor is said plainly");

    s.valid = true;
    s.chip = "ina226";
    s.probe_up = true;
    s.mean_ua = 212300;
    s.vbus_mv = 3291;
    s.vbus_min_mv = 3120;
    s.sag = true;
    std::memset(s.label, 'x', 32);
    composeLines(s, l);
    check(std::strcmp(l.head, "ina226 3.29V UP") == 0, "display: head line");
    check(std::strcmp(l.big, "212.3 mA") == 0, "display: mean in the big line");
    check(std::strstr(l.flags, "Vmin 3.12 SAG") != nullptr, "display: sag is flagged");
    check(std::strlen(l.label) == kLineCap - 1, "display: a long label is cut to the panel width");

    s.label[0] = '\0';
    s.stream = false;
    composeLines(s, l);
    check(std::strcmp(l.label, "idle") == 0, "display: no label, not streaming = idle");
}

void testProbePower()
{
    ProbePower p;
    check(p.update(3300, 0) == ProbePower::Change::None, "not up on the first sample");
    check(p.update(3300, ProbePower::kUpHoldUs - 1) == ProbePower::Change::None, "not up before the hold time");
    check(p.update(3300, ProbePower::kUpHoldUs) == ProbePower::Change::Up, "up after the hold time");
    check(p.update(2800, 300000) == ProbePower::Change::None, "a dip above the down threshold is tolerated");
    check(p.update(-1, 310000) == ProbePower::Change::Down, "a failed read counts as down");

    ProbePower q;
    q.update(3300, 0);
    q.update(1000, 100000);  /* bounce resets the hold */
    q.update(3300, 150000);
    check(q.update(3300, 300000) == ProbePower::Change::None, "a bounce restarts the hold time");
    check(q.update(5000, 350000) == ProbePower::Change::Up, "a 5 V feed also counts as up");
}

}  // namespace

int main()
{
    testDownlink();
    testUplink();
    testCommands();
    testWindow();
    testDecode();
    testDisplay();
    testProbePower();
    std::printf("meter: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
