/*
 * link_reconcile.h — which probe lanes to stop when the link becomes Ready.
 *
 * A deck that rebooted or reconnected believes its lanes are idle, but the
 * probe keeps running whatever it had and answers new starts with `busy`
 * (WORKLOG 2026-11-20). The deck's belief wins: stop every lane it does not
 * think is running. `stop` is always acked, even when idle (OCP-SPEC §6).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ocp.h"

#include <string>

namespace app {

inline const char *reconcileStopLane(bool deck_phy_active, bool deck_lora_active)
{
    if (!deck_phy_active && !deck_lora_active) return OCP_LANE_ALL;
    if (!deck_phy_active) return OCP_LANE_PHY;
    if (!deck_lora_active) return OCP_LANE_LORA;
    return nullptr;
}

/* The reverse case: the probe rebooted and lost lanes the deck still
 * believes in. Only an explicit idle report clears; a missing key does not. */
struct BeliefClear { bool phy; bool lora; };

inline BeliefClear reconcileFromStatus(bool deck_phy_active, bool deck_lora_active,
                                       const std::string &probe_owner,
                                       const std::string &probe_lora)
{
    return {deck_phy_active && probe_owner == "none",
            deck_lora_active && !probe_lora.empty() && probe_lora != "rx" && probe_lora != "sweep"};
}

}  // namespace app
