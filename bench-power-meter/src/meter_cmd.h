/*
 * meter_cmd.h — the meter's own host commands (the `#` lines). The command
 * set is documented in this directory's README. Pure C++, host-tested.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace meter {

enum class CmdKind { Invalid, StreamOn, StreamOff, Window, Shunt, Sag, Mark, Info, Scan, Panel };

struct Cmd {
    CmdKind kind = CmdKind::Invalid;
    uint32_t value = 0;             /* Window: ms, Shunt: µΩ, Sag: mV, Panel: 0 ssd1306 / 1 sh1106 */
    char label[33] = {};            /* Mark */
    const char *err = nullptr;      /* Invalid: why */
};

constexpr uint32_t kWindowMinMs = 10, kWindowMaxMs = 10000;
constexpr uint32_t kShuntMinUohm = 1000, kShuntMaxUohm = 1000000;
constexpr uint32_t kSagMaxMv = 36000;   /* INA226 bus range */

/* `line` is the text after '#'. Never reads past its terminator. */
Cmd parseCommand(const char *line, bool overflow);

}  // namespace meter
