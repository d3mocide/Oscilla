/*
 * line_reader_test.cpp — host test for debug::LineReader. Adversarial first,
 * per AGENTS.md §6: byte-at-a-time chunking and an overlong line before
 * anything that merely looks like a well-formed command.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>
#include <vector>

#include "debug/line_reader.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

std::vector<std::string> feedAll(debug::LineReader &r, const std::string &wire)
{
    std::vector<std::string> out;
    r.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](const std::string &line) { out.push_back(line); });
    return out;
}

}  // namespace

int main()
{
    {
        debug::LineReader r;
        auto out = feedAll(r, "scan\n");
        check(out.size() == 1 && out[0] == "scan", "a plain command line parses");
    }
    {
        debug::LineReader r;
        auto out = feedAll(r, "scan\r\n");
        check(out.size() == 1 && out[0] == "scan", "a trailing \\r is trimmed");
    }
    {
        debug::LineReader r;
        auto out = feedAll(r, "\n");
        check(out.size() == 1 && out[0].empty(), "an empty line still surfaces — filtering is the caller's job");
    }
    {
        /* Chunk-invariant, byte at a time: the shape a human typing over
         * USB Serial actually produces. */
        debug::LineReader r;
        std::string wire = "wardrive\n";
        std::vector<std::string> out;
        for (char c : wire) {
            uint8_t b = static_cast<uint8_t>(c);
            r.feed(&b, 1, [&](const std::string &line) { out.push_back(line); });
        }
        check(out.size() == 1 && out[0] == "wardrive", "a line typed one byte at a time still parses whole");
    }
    {
        debug::LineReader r;
        std::string wire = "scan\nstatus\nstop lora\n";
        auto out = feedAll(r, wire);
        check(out.size() == 3 && out[0] == "scan" && out[1] == "status" && out[2] == "stop lora",
              "multiple lines in one feed() come out in order");
    }
    {
        debug::LineReader r;
        std::string overlong(debug::LineReader::kMaxLineLen + 40, 'A');
        std::string wire = overlong + "\nscan\n";   /* recovery: a good line right after */
        auto out = feedAll(r, wire);
        check(out.size() == 1 && out[0] == "scan",
              "an overlong line is bounded and dropped without wedging the next one");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "line reader test FAILED" : "line reader test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
