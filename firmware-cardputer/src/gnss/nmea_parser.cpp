/*
 * nmea_parser.cpp — see nmea_parser.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gnss/nmea_parser.h"

namespace gnss {

namespace {

bool isHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return c - 'a' + 10;
}

/* Splits "GNGGA" into talker="GN", type="GGA": talker is always the first
 * two characters of a standard sentence header (NMEA 0183 §5.2.1.1). A
 * proprietary "$P..." header has no talker at all; type takes the whole
 * remainder so it's still visible to a caller without crashing on it. */
void splitHeader(const std::string &head, std::string &talker, std::string &type)
{
    if (head.size() >= 1 && head[0] == 'P') {
        talker.clear();
        type = head;
        return;
    }
    if (head.size() < 2) {
        talker.clear();
        type = head;
        return;
    }
    talker = head.substr(0, 2);
    type = head.substr(2);
}

}  // namespace

void NmeaParser::feed(const uint8_t *data, size_t len, const Sink &sink)
{
    for (size_t i = 0; i < len; ++i) {
        char b = static_cast<char>(data[i]);
        if (b == '\n') {
            std::string line;
            line.swap(buf_);
            bool was_overlong = overlong_;
            overlong_ = false;
            if (!was_overlong) feedLine(line, sink);
            continue;
        }
        if (overlong_) continue;   /* discard to end of line */
        buf_.push_back(b);
        if (buf_.size() > kMaxLineLen) {
            overlong_ = true;
            buf_.clear();
        }
    }
}

void NmeaParser::feedLine(const std::string &raw, const Sink &sink)
{
    std::string line(raw);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.empty() || line.front() != '$') return;   /* not a sentence: silently dropped */

    size_t star = line.rfind('*');
    if (star == std::string::npos || star + 2 >= line.size()) return;   /* no checksum */
    if (!isHex(line[star + 1]) || !isHex(line[star + 2])) return;

    uint8_t want = static_cast<uint8_t>(hexVal(line[star + 1]) * 16 + hexVal(line[star + 2]));
    uint8_t got = 0;
    for (size_t i = 1; i < star; ++i) got ^= static_cast<uint8_t>(line[i]);
    if (got != want) return;   /* corrupt on the wire: dropped, not surfaced as noise */

    std::string body = line.substr(1, star - 1);   /* between '$' and '*' */
    size_t comma = body.find(',');
    std::string head = (comma == std::string::npos) ? body : body.substr(0, comma);
    if (head.empty()) return;

    Sentence s;
    splitHeader(head, s.talker, s.type);

    size_t pos = (comma == std::string::npos) ? body.size() : comma + 1;
    while (pos <= body.size()) {
        size_t next = body.find(',', pos);
        if (next == std::string::npos) {
            s.fields.push_back(body.substr(pos));
            break;
        }
        s.fields.push_back(body.substr(pos, next - pos));
        pos = next + 1;
    }

    sink(s);
}

}  // namespace gnss
