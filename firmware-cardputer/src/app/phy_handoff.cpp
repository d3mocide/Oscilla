/*
 * phy_handoff.cpp — see phy_handoff.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/phy_handoff.h"

namespace app {

PhyHandoff::Request PhyHandoff::request(bool phy_active, bool stop_pending, Start start)
{
    if (!phy_active) return Request::StartNow;

    const bool already_queued = queued();
    queued_ = std::move(start);
    return (stop_pending || already_queued) ? Request::WaitForStop : Request::SendStop;
}

bool PhyHandoff::takeAfterStop(Start *out)
{
    if (!queued_) return false;
    *out = std::move(queued_);
    return true;
}

}  // namespace app
