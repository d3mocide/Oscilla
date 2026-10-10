/* Plain snapshot of measured deck/probe diagnostics. */
#pragma once
#include "ocp/ocp_client.h"
namespace model {
struct SystemStatus {
    bool info = false, probe_valid = false;
    ocp::LinkState link = ocp::LinkState::Disconnected;
    ocp::ProbeInfo probe;
    ocp::ClientStats stats;
    uint32_t deck_heap = 0, deck_min = 0, probe_heap = 0, probe_total = 0;
    uint64_t probe_uptime = 0;
};
}
