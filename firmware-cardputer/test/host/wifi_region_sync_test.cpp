#include "app/wifi_region_sync.h"
#include <cassert>
#include <iostream>
#include <string>
int main(){
    using app::RegionState;
    /* Saved value: known codes kept, anything else falls back to US. */
    assert(std::string(app::regionOrDefault("CA").code) == "CA");
    assert(std::string(app::regionOrDefault("").code) == "US");
    assert(std::string(app::regionOrDefault("ca").code) == "US");
    assert(std::string(app::regionOrDefault("ZZ").code) == "US");
    /* Cycling visits every region and wraps both ways. */
    size_t n = 0;
    const ocp_wifi_region_t *t = ocp_wifi_region_table(&n);
    std::string code = "US";
    for (size_t i = 0; i < n; i++) code = app::cycleRegion(code, 1).code;
    assert(code == "US");
    assert(std::string(app::cycleRegion(t[0].code, -1).code) == t[n - 1].code);
    assert(std::string(app::cycleRegion(t[n - 1].code, 1).code) == t[0].code);
    assert(std::string(app::cycleRegion("US", 1).code) == t[1].code);
    /* Push only on a real disagreement with an idle PHY lane. */
    assert(app::regionPushDue("CA", "US", true, false, 0));
    assert(!app::regionPushDue("US", "US", true, false, 0));
    assert(!app::regionPushDue("CA", "US", false, false, 0));
    /* A probe without region= predates set_region: never push. */
    assert(!app::regionPushDue("CA", "", true, false, 0));
    /* A refused push is retried, but not every tick. */
    assert(!app::regionPushDue("CA", "US", true, true, app::kRegionRetryMs - 1));
    assert(app::regionPushDue("CA", "US", true, true, app::kRegionRetryMs));
    /* What Settings shows. */
    assert(app::regionState(false, "CA", "CA") == RegionState::Offline);
    assert(app::regionState(true, "CA", "") == RegionState::Unsupported);
    assert(app::regionState(true, "CA", "CA") == RegionState::Applied);
    assert(app::regionState(true, "CA", "US") == RegionState::Pending);
    std::cout<<"wifi region sync: saved-code fallback, cycling, push only on idle disagreement, retry spacing, settings state passed\n";
}
