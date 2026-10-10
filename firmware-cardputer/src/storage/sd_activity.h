/* File write acceptance and indicator state; flush is not readback proof. */
#pragma once
#include <cstddef>
#include <cstdint>

namespace storage {
enum class SdIndicator { Off, Ready, Write, Fault };

struct SdActivity {
    uint32_t writes = 0, errors = 0, last_write_ms = 0;
    uint64_t bytes = 0;
    bool failed = false;

    /* Call with the actual File write result; excludes zero-byte metadata. */
    void record(size_t requested, size_t accepted, uint32_t now_ms)
    {
        if (!requested) return;
        bytes += accepted;
        if (accepted != requested) { ++errors; failed = true; return; }
        ++writes;
        last_write_ms = now_ms;
        failed = false;
    }

    SdIndicator indicator(bool ready, uint32_t now_ms) const
    {
        if (failed) return SdIndicator::Fault;
        if (!ready) return SdIndicator::Off;
        if (writes && now_ms - last_write_ms < 1500) return SdIndicator::Write;
        return SdIndicator::Ready;
    }
};

/* Shared across loggers; all record calls use the SD bus lock. */
inline SdActivity &sdActivity() { static SdActivity activity; return activity; }
}
