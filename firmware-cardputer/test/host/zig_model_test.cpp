/* zig_model_test.cpp — host test for [ZIG] rows. */
#include <cstdio>
#include <string>
#include "model/zig_model.h"
#include "ocp/ocp_parser.h"
static int failed;
static void check(bool v, const char *s) { std::printf("  %s  %s\n", v ? "PASS" : "FAIL", s); if (!v) failed = 1; }
static ocp::Item frame(const std::string &wire) { ocp::Parser p; ocp::Item out; p.feed((const uint8_t *)wire.data(), wire.size(), [&](ocp::Item &&i) { if (i.kind == ocp::ItemKind::Frame) out = std::move(i); }); return out; }
int main() {
    model::ZigModel m; m.begin();
    m.absorb(frame("[ZIG] BEGIN n=2 pans=1 nodes=1\n[ZIG] \"pan\",\"1a2b\",\"802154\",\"mac\",\"0001\",\"1\",\"-60\",\"91\"\n[ZIG] \"node\",\"1a2b\",\"1234\",\"\",\"unknown\",\"-60\",\"91\",\"10\"\n[ZIG] END\n"));
    check(m.pans().size() == 1 && m.nodes().size() == 1, "PAN and node rows accepted");
    check(m.nodes()[0].pan == "1a2b" && m.nodes()[0].rssi == -60, "node fields retained");
    m.absorb(frame("[ZIG] BEGIN n=1\n[ZIG] \"node\",\"bad\",\"1234\",\"\",\"unknown\",\"-60\",\"91\",\"10\"\n[ZIG] END\n"));
    check(m.nodes().empty() && m.malformedRows() == 1, "malformed hostile row rejected");
    m.stop(); check(!m.active(), "stop preserves table state semantics");
    std::printf("\nzig model test %s\n", failed ? "FAILED" : "OK"); return failed;
}
