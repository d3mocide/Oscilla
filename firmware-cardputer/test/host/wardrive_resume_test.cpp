/* Host tests for app::WardriveResume. */
#include "app/wardrive_resume.h"

#include <cassert>
#include <iostream>

int main()
{
    using app::WardriveResume;
    const uint32_t settle = WardriveResume::kSettleMs;

    // Not wanted or PHY busy: never due.
    WardriveResume r;
    assert(!r.due(false, true, true, 0) && !r.due(false, true, true, 2 * settle));
    assert(!r.due(true, false, true, 0) && !r.due(true, false, true, 2 * settle));

    // Clear lane: due only after the full settle window.
    assert(!r.due(true, true, true, 1000));
    assert(!r.due(true, true, true, 1000 + settle - 1));
    assert(r.due(true, true, true, 1000 + settle));

    // A PHY blip restarts the wait (user started another tool).
    WardriveResume b;
    assert(!b.due(true, true, true, 0));
    assert(!b.due(true, false, true, settle - 10));
    assert(!b.due(true, true, true, settle));
    assert(!b.due(true, true, true, 2 * settle - 1));
    assert(b.due(true, true, true, 2 * settle));

    // A busy client (2 s status poll, found on hardware) defers but never resets.
    WardriveResume p;
    assert(!p.due(true, true, true, 0));
    for (uint32_t t = 2000; t <= 3 * settle; t += 2000) assert(!p.due(true, true, false, t));
    assert(p.due(true, true, true, 3 * settle + 1));
    WardriveResume q;
    assert(!q.due(true, true, true, 0));
    assert(!q.due(true, true, false, settle - 1));
    assert(q.due(true, true, true, settle));

    // reset() after a start attempt waits a full window before retrying.
    b.reset();
    assert(!b.due(true, true, true, 2 * settle + 1));
    assert(b.due(true, true, true, 3 * settle + 1));

    // millis() wrap.
    WardriveResume w;
    assert(!w.due(true, true, true, 0xFFFFFF00u));
    assert(w.due(true, true, true, 0xFFFFFF00u + settle));

    std::cout << "wardrive resume: settle window, PHY restart, client-busy defer, retry backoff and clock wrap passed\n";
    return 0;
}
