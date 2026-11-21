/* Host tests for app::IdleSleep (WP-5). */
#include "app/idle_sleep.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    using app::IdleSleep;
    using app::IdleTransition;

    // Presets, validation, sanitization, and labels.
    assert(IdleSleep::isValidTimeoutS(0));
    assert(IdleSleep::isValidTimeoutS(30));
    assert(IdleSleep::isValidTimeoutS(60));
    assert(IdleSleep::isValidTimeoutS(120));
    assert(IdleSleep::isValidTimeoutS(300));
    assert(!IdleSleep::isValidTimeoutS(15));
    assert(!IdleSleep::isValidTimeoutS(600));
    assert(IdleSleep::sanitizeTimeoutS(60) == 60);
    assert(IdleSleep::sanitizeTimeoutS(99) == 0);
    assert(std::strcmp(IdleSleep::label(0), "OFF") == 0);
    assert(std::strcmp(IdleSleep::label(30), "30s") == 0);
    assert(std::strcmp(IdleSleep::label(60), "60s") == 0);
    assert(std::strcmp(IdleSleep::label(120), "2m") == 0);
    assert(std::strcmp(IdleSleep::label(300), "5m") == 0);
    assert(std::strcmp(IdleSleep::label(77), "OFF") == 0);

    // Default OFF never enters sleep regardless of elapsed time.
    IdleSleep s;
    s.begin(1000);
    assert(s.timeoutS() == 0 && !s.sleeping());
    assert(s.tick(1000000) == IdleTransition::None);
    assert(!s.sleeping());

    // Cycling forward and backward wraps through all presets.
    assert(s.cycleTimeoutS(1, 2000) == 30);
    assert(s.cycleTimeoutS(1, 2000) == 60);
    assert(s.cycleTimeoutS(1, 2000) == 120);
    assert(s.cycleTimeoutS(1, 2000) == 300);
    assert(s.cycleTimeoutS(1, 2000) == 0);
    assert(s.cycleTimeoutS(-1, 2000) == 300);

    // Invalid timeout rejected without disturbing state.
    assert(!s.setTimeoutS(45, 3000));
    assert(s.timeoutS() == 300);

    // Exact threshold transition and wake-key swallowing.
    assert(s.setTimeoutS(30, 10000));
    assert(s.tick(39999) == IdleTransition::None);
    assert(!s.sleeping());
    assert(s.tick(40000) == IdleTransition::EnterSleep);
    assert(s.sleeping());
    // Subsequent ticks while asleep stay quiet.
    assert(s.tick(50000) == IdleTransition::None);
    assert(s.sleeping());

    // First key press wakes and MUST return true (swallowed).
    assert(s.onActivity(55000) == true);
    assert(!s.sleeping());
    // Immediate next key press passes through (returns false).
    assert(s.onActivity(55100) == false);
    assert(!s.sleeping());
    // Activity reset the timer from 55100 -> sleeps at 85100.
    assert(s.tick(85099) == IdleTransition::None);
    assert(s.tick(85100) == IdleTransition::EnterSleep);
    assert(s.sleeping());

    // Disabling timeout (setting 0) while sleeping wakes cleanly on next tick.
    assert(s.setTimeoutS(0, 86000));
    assert(s.tick(86001) == IdleTransition::Wake);
    assert(!s.sleeping());
    assert(s.tick(200000) == IdleTransition::None);

    // Unsigned 32-bit millis() wrap-around across UINT32_MAX.
    const uint32_t near_wrap = UINT32_MAX - 10000;
    assert(s.setTimeoutS(30, near_wrap));
    assert(s.tick(near_wrap + 10000) == IdleTransition::None);
    assert(s.tick(19998) == IdleTransition::None);
    assert(s.tick(19999) == IdleTransition::EnterSleep);
    assert(s.sleeping());
    assert(s.onActivity(20500) == true);
    assert(!s.sleeping());

    std::cout << "idle sleep: presets, threshold, wake-key swallow, disable-while-asleep, and millis wrap passed\n";
    return 0;
}
