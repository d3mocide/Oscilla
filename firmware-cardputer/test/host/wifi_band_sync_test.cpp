/*
 * wifi_band_sync_test.cpp — host tests for app/wifi_band_sync.h (OCP-SPEC §10.9).
 *
 * SPDX-License-Identifier: MIT
 */

#include "app/wifi_band_sync.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond, what) do { if (!(cond)) { std::printf("FAIL: %s\n", what); failures++; } } while (0)

int main()
{
    CHECK(app::bandOrDefault("") == "auto", "empty falls back to auto");
    CHECK(app::bandOrDefault("AUTO") == "auto", "case-sensitive fallback");
    CHECK(app::bandOrDefault("2.4") == "auto", "invalid 2.4 falls back");
    CHECK(app::bandOrDefault("24") == "24", "24 preserved");
    CHECK(app::bandOrDefault("5") == "5", "5 preserved");
    CHECK(app::bandOrDefault("auto") == "auto", "auto preserved");

    CHECK(app::cycleBand("auto", 1) == "24", "cycle auto -> 24");
    CHECK(app::cycleBand("24", 1) == "5", "cycle 24 -> 5");
    CHECK(app::cycleBand("5", 1) == "auto", "cycle 5 -> auto");
    CHECK(app::cycleBand("auto", -1) == "5", "reverse cycle auto -> 5");
    CHECK(app::cycleBand("bogus", 1) == "24", "unknown starts at auto -> 24");

    CHECK(std::string(app::bandDisplayLabel("auto")) == "AUTO", "label auto");
    CHECK(std::string(app::bandDisplayLabel("24")) == "2.4G", "label 24");
    CHECK(std::string(app::bandDisplayLabel("5")) == "5G", "label 5");

    /* Push policy: only when probe supports band=, disagrees, and PHY idle. */
    CHECK(!app::bandPushDue("24", "", true, false, 0), "no push to old probe without band=");
    CHECK(!app::bandPushDue("24", "24", true, false, 0), "no push when already matched");
    CHECK(!app::bandPushDue("24", "auto", false, false, 0), "no push while PHY busy");
    CHECK(app::bandPushDue("24", "auto", true, false, 0), "push immediately on first disagreement");
    CHECK(!app::bandPushDue("24", "auto", true, true, app::kBandRetryMs - 1), "respect retry spacing");
    CHECK(app::bandPushDue("24", "auto", true, true, app::kBandRetryMs), "retry after interval");

    CHECK(app::bandState(false, "24", "auto") == app::BandState::Offline, "offline");
    CHECK(app::bandState(true, "24", "") == app::BandState::Unsupported, "unsupported");
    CHECK(app::bandState(true, "24", "24") == app::BandState::Applied, "applied");
    CHECK(app::bandState(true, "24", "auto") == app::BandState::Pending, "pending");

    if (failures) { std::printf("wifi band sync: %d failure(s)\n", failures); return 1; }
    std::printf("wifi band sync: saved-code fallback, cycling, push only on idle disagreement, retry spacing passed\n");
    return 0;
}
