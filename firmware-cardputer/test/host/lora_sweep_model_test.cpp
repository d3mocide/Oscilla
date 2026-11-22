/* Host tests for model::LoraSweepModel (WP-8). */
#include "model/lora_sweep_model.h"
#include "ocp.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

ocp::Item makeSweep(uint32_t start_khz, uint16_t step_khz, uint8_t bins, const std::string &csv)
{
    ocp::Item it;
    it.kind = ocp::ItemKind::Event;
    it.kv = {{OCP_K_KIND, OCP_EVT_KIND_LORA_SWEEP},
             {OCP_K_START_KHZ, std::to_string(start_khz)},
             {OCP_K_STEP_KHZ, std::to_string(step_khz)},
             {OCP_K_BINS, std::to_string(bins)},
             {OCP_K_RSSI, csv}};
    return it;
}

}  // namespace

int main()
{
    model::LoraSweepModel m;
    assert(!m.active() && !m.hasData());

    m.begin(902000, 400, 8);
    assert(m.active() && !m.hasData());

    // First valid sweep: spike at bin 3 (-62 dBm), floor around -112 dBm.
    assert(m.absorbEvent(makeSweep(902000, 400, 8, "-112,-111,-110,-62,-113,-112,-111,-114"), 1000));
    assert(m.hasData() && m.sweeps() == 1 && m.bins() == 8);
    assert(m.peakBin() == 3 && m.maxPeakDbm() == -62);
    assert(m.binFreqKhz(3) == 903200);
    assert(m.floorDbm() == -112);
    assert(m.historyCount() == 1 && m.historyDbm(0, 3) == -62);

    // Second sweep: spike moves to bin 5 (-70 dBm); peak-hold preserves bin 3 (-62) AND records bin 5 (-70).
    assert(m.absorbEvent(makeSweep(902000, 400, 8, "-115,-114,-113,-110,-112,-70,-113,-114"), 1500));
    assert(m.sweeps() == 2 && m.peakBin() == 3 && m.maxPeakDbm() == -62);
    assert(m.peakDbm(5) == -70 && m.currentDbm(3) == -110);
    assert(m.historyCount() == 2 && m.historyDbm(0, 5) == -70 && m.historyDbm(1, 3) == -62);

    // clearPeak() resets envelope to current sweep.
    m.clearPeak();
    assert(m.peakBin() == 5 && m.maxPeakDbm() == -70);

    // Malformed sweeps rejected without corrupting prior state.
    assert(!m.absorbEvent(makeSweep(902000, 400, 8, "-110,-110"), 2000));                // wrong bin count
    assert(!m.absorbEvent(makeSweep(902000, 400, 8, "-110,-110,-110,5,-110,-110,-110,-110"), 2000)); // > 0 dBm
    assert(!m.absorbEvent(makeSweep(959000, 400, 8, "-110,-110,-110,-110,-110,-110,-110,-110"), 2000)); // > 960 MHz
    assert(m.malformed() == 3 && m.sweeps() == 2 && m.peakBin() == 5);

    // Ring wrap beyond 24 history rows stays bounded and ordered newest-first.
    for (int i = 0; i < 30; ++i) {
        int v = -100 + (i % 20);
        std::string row = std::to_string(v) + ",-112,-112,-112,-112,-112,-112,-112";
        assert(m.absorbEvent(makeSweep(902000, 400, 8, row), static_cast<uint32_t>(3000 + i * 100)));
    }
    assert(m.historyCount() == model::LoraSweepModel::kHistoryRows);
    assert(m.historyDbm(0, 0) == -100 + (29 % 20));
    assert(m.historyDbm(1, 0) == -100 + (28 % 20));

    std::cout << "lora sweep model: parsing, peak-hold, floor, bounds and 24-row waterfall ring passed\n";
    return 0;
}
