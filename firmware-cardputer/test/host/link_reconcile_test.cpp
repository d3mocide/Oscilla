#include "app/link_reconcile.h"
#include <cassert>
#include <cstring>
#include <iostream>
static bool is(const char *got, const char *want) { return got && std::strcmp(got, want) == 0; }
int main(){
    /* Fresh deck, probe may still run both lanes: stop everything. */
    assert(is(app::reconcileStopLane(false, false), OCP_LANE_ALL));
    /* Deck still logging LoRa across a link drop: free only the PHY. */
    assert(is(app::reconcileStopLane(false, true), OCP_LANE_PHY));
    /* Deck still owns a PHY tool: leave it, clear LoRa leftovers. */
    assert(is(app::reconcileStopLane(true, false), OCP_LANE_LORA));
    /* Deck believes both run: nothing to reconcile. */
    assert(app::reconcileStopLane(true, true) == nullptr);
    /* Probe rebooted under a busy deck: both beliefs cleared. */
    auto c = app::reconcileFromStatus(true, true, "none", "idle");
    assert(c.phy && c.lora);
    /* Probe still running what the deck believes: keep both. */
    c = app::reconcileFromStatus(true, true, "wifi", "rx");
    assert(!c.phy && !c.lora);
    /* Deck idle: nothing to clear, whatever the probe says. */
    c = app::reconcileFromStatus(false, false, "none", "idle");
    assert(!c.phy && !c.lora);
    /* LoRa radio gone counts as not listening. */
    c = app::reconcileFromStatus(false, true, "none", "absent");
    assert(!c.phy && c.lora);
    /* Old probe without the keys: never guess. */
    c = app::reconcileFromStatus(true, true, "", "");
    assert(!c.phy && !c.lora);
    std::cout<<"link reconcile: stale probe lanes stopped, stale deck beliefs cleared, owned lanes kept passed\n";
}
