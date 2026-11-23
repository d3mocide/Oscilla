/* Host tests for app::WardriveResume. */
#include "app/wardrive_resume.h"

#include <cassert>
#include <iostream>

int main()
{
    using app::WardriveResume;
    const uint32_t settle = WardriveResume::kSettleMs;

    // Not wanted or lane busy: never due.
    WardriveResume r;
    assert(!r.due(false, true, 0) && !r.due(false, true, 2 * settle));
    assert(!r.due(true, false, 0) && !r.due(true, false, 2 * settle));

    // Clear lane: due only after the full settle window.
    assert(!r.due(true, true, 1000));
    assert(!r.due(true, true, 1000 + settle - 1));
    assert(r.due(true, true, 1000 + settle));

    // A busy blip restarts the wait (user started another tool).
    WardriveResume b;
    assert(!b.due(true, true, 0));
    assert(!b.due(true, false, settle - 10));
    assert(!b.due(true, true, settle));
    assert(!b.due(true, true, 2 * settle - 1));
    assert(b.due(true, true, 2 * settle));

    // reset() after a start attempt waits a full window before retrying.
    b.reset();
    assert(!b.due(true, true, 2 * settle + 1));
    assert(b.due(true, true, 3 * settle + 1));

    // millis() wrap.
    WardriveResume w;
    assert(!w.due(true, true, 0xFFFFFF00u));
    assert(w.due(true, true, 0xFFFFFF00u + settle));

    std::cout << "wardrive resume: settle window, busy restart, retry backoff and clock wrap passed\n";
    return 0;
}
