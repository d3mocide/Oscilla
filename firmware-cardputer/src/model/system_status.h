/* Plain snapshot of measured deck/probe diagnostics. */
#pragma once
#include "ocp/ocp_client.h"
#include "model/gnss_model.h"
namespace model {
struct SystemStatus {
    bool info = false, probe_valid = false;
    ocp::LinkState link = ocp::LinkState::Disconnected;
    ocp::ProbeInfo probe;
    ocp::ClientStats stats;
    uint32_t deck_heap = 0, deck_min = 0, probe_heap = 0, probe_total = 0;
    uint32_t deck_total = 0;
    bool sd_ready = false;
    GnssState gnss_state = GnssState::NoUartData;
    uint32_t probe_sample_ms = 0, probe_sequence = 0, probe_epoch = 0;
    std::string probe_owner, probe_lora, phy_task;
    bool drive_logging = false, lora_logging = false;
    uint64_t probe_uptime = 0;
    int battery_pct = -1;
    bool battery_charging = false;
};
}
