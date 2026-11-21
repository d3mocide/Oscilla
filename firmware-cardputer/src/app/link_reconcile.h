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

namespace app {

inline const char *reconcileStopLane(bool deck_phy_active, bool deck_lora_active)
{
    if (!deck_phy_active && !deck_lora_active) return OCP_LANE_ALL;
    if (!deck_phy_active) return OCP_LANE_PHY;
    if (!deck_lora_active) return OCP_LANE_LORA;
    return nullptr;
}

}  // namespace app
