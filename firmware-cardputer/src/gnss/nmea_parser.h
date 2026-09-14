/*
 * nmea_parser.h — byte-exact NMEA 0183 reader for the deck's GNSS UART
 * (DESIGN §9.1, Rev D §6). Framework-agnostic; host-tested in
 * test/host/nmea_parser_test.cpp.
 *
 * Never throws and never grows past its bound: anything that doesn't parse
 * as a checksum-valid `$...*hh` sentence is silently dropped, same
 * never-trust-the-wire posture as ocp::Parser. A checksum-valid sentence of
 * *any* type is still handed to the sink even if its talker/type isn't one
 * GnssModel interprets — that's what lets the model tell "no fix" (sentences
 * arriving, none of them a fix) apart from "no UART data" (Rev D §6).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gnss {

/* One checksum-valid sentence, split but not otherwise interpreted. Field
 * indices follow the NMEA 0183 layout for that talker+type; GnssModel owns
 * knowing what GGA/RMC fields mean. */
struct Sentence {
    std::string talker;                 /* e.g. "GN", "GP" */
    std::string type;                   /* e.g. "GGA", "RMC", "GSV" */
    std::vector<std::string> fields;    /* between type and checksum, in order */
};

class NmeaParser {
public:
    /* NMEA 0183 caps a sentence at 82 bytes incl. '$'/CRLF; double it for
     * slack without letting one bad line grow unbounded. */
    static constexpr size_t kMaxLineLen = 164;

    using Sink = std::function<void(const Sentence &)>;

    /* Raw serial bytes, any chunking. */
    void feed(const uint8_t *data, size_t len, const Sink &sink);

    /* One line, without its terminator. Exposed for tests; feed() calls it
     * per '\n'. */
    void feedLine(const std::string &line, const Sink &sink);

private:
    std::string buf_;
    bool overlong_ = false;
};

}  // namespace gnss
