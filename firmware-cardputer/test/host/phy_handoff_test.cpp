/*
 * phy_handoff_test.cpp — acknowledgement ordering for PHY tool transitions.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>

#include "app/phy_handoff.h"

namespace {

int g_pass;
int g_fail;

void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

}  // namespace

int main()
{
    app::PhyHandoff h;
    int starts = 0;
    uint32_t started_at = 0;

    check(h.request(false, false, [&](uint32_t now) { starts++; started_at = now; }) ==
              app::PhyHandoff::Request::StartNow &&
              !h.queued() && starts == 0,
          "an idle PHY sends the requested tool without a synthetic stop");

    check(h.request(true, false, [&](uint32_t now) { starts++; started_at = now; }) ==
              app::PhyHandoff::Request::SendStop && h.queued() && starts == 0,
          "an active PHY queues the next tool behind a scoped stop");

    app::PhyHandoff::Start start;
    check(h.takeAfterStop(&start) && !h.queued() && starts == 0,
          "the queued tool cannot run until the stop acknowledgement is handled");
    start(42);
    check(!h.queued() && starts == 1 && started_at == 42,
          "the queued tool runs exactly after the stop acknowledgement");

    check(h.request(true, false, [&](uint32_t) { starts += 10; }) ==
              app::PhyHandoff::Request::SendStop,
          "a second active transition asks for one stop");
    check(h.request(true, true, [&](uint32_t) { starts += 100; }) ==
              app::PhyHandoff::Request::WaitForStop,
          "a second request waits for the stop already in flight");
    check(h.takeAfterStop(&start), "a latest queued tool is available after the shared stop");
    start(99);
    check(starts == 101, "the newest requested tool replaces the stale destination");

    h.request(true, false, [&](uint32_t) { starts += 1000; });
    h.clear();
    check(!h.takeAfterStop(&start) && starts == 101,
          "link loss or navigation cancellation cannot start a stale PHY tool");

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "phy handoff test FAILED" : "phy handoff test OK",
                g_pass, g_fail);
    return g_fail ? 1 : 0;
}
