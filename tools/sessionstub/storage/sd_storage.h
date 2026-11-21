#pragma once
#include <cstdint>
namespace storage {
bool ready();
bool lock(uint32_t timeout_ms = 200);
void unlock();
} // namespace storage
