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
    std::cout<<"link reconcile: stale probe lanes stopped, deck-owned lanes kept passed\n";
}
