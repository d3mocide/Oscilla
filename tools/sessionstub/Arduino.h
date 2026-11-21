#pragma once
#include <cstdint>
extern uint32_t session_test_now;
inline uint32_t millis() {
    return session_test_now;
}
