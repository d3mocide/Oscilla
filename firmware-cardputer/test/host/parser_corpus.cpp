/*
 * parser_corpus.cpp — host harness for the deck parser.
 *
 * Reads one byte stream, prints each parsed item as a JSON line in the same
 * canonical form as tools/ocp_fuzz.py. Also re-parses byte-by-byte and fails
 * if the result differs or a cap is ever exceeded.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ocp/ocp_csv.h"
#include "ocp/ocp_parser.h"

namespace {

std::string hex(const std::string &s)
{
    static const char d[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : s) { out += d[c >> 4]; out += d[c & 15]; }
    return out;
}

std::string jsonStr(const std::string &s)   /* tags only: always ASCII markers */
{
    std::string out = "\"";
    for (char c : s) { if (c == '"' || c == '\\') out += '\\'; out += c; }
    return out + "\"";
}

std::string canon(const ocp::Item &it)
{
    using K = ocp::ItemKind;
    auto kv = [&]() {
        std::string o = "{";
        for (size_t i = 0; i < it.kv.size(); ++i)
            o += (i ? "," : "") + jsonStr(it.kv[i].key) + ":\"" + hex(it.kv[i].value) + "\"";
        return o + "}";
    };
    switch (it.kind) {
    case K::Frame: {
        std::string rows = "[";
        for (size_t i = 0; i < it.rows.size(); ++i) rows += (i ? ",\"" : "\"") + hex(it.rows[i]) + "\"";
        return "{\"t\":\"frame\",\"tag\":" + jsonStr(it.tag) + ",\"compact\":" +
               (it.compact ? "true" : "false") + ",\"kv\":" + kv() + ",\"rows\":" + rows + "]}";
    }
    case K::Event: return "{\"t\":\"evt\",\"kv\":" + kv() + "}";
    case K::Error: return "{\"t\":\"err\",\"kv\":" + kv() + "}";
    case K::Pong:  return "{\"t\":\"pong\"}";
    case K::Noise: return "{\"t\":\"noise\"}";
    }
    return "{}";
}

std::vector<std::string> run(const std::vector<uint8_t> &data, size_t chunk, bool &bounded)
{
    ocp::Parser p;
    std::vector<std::string> out;
    auto sink = [&](ocp::Item &&it) { out.push_back(canon(it)); };
    for (size_t i = 0; i < data.size(); i += chunk) {
        size_t n = std::min(chunk, data.size() - i);
        p.feed(data.data() + i, n, sink);
        if (p.bufferedBytes() > OCP_MAX_LINE_LEN || p.openRows() > OCP_MAX_FRAME_ROWS) bounded = false;
    }
    return out;
}

}  // namespace

/* --csv FILE: one hex-encoded row per line -> JSON list of hex fields, or null. */
int csvMode(const char *path)
{
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::string row;
        for (size_t i = 0; i + 1 < line.size(); i += 2) row += (char)std::stoi(line.substr(i, 2), nullptr, 16);
        std::vector<std::string> fields;
        if (!ocp::splitCsvRow(row, fields)) { std::printf("null\n"); continue; }
        std::string out = "[";
        for (size_t i = 0; i < fields.size(); ++i) out += (i ? ",\"" : "\"") + hex(fields[i]) + "\"";
        std::printf("%s]\n", out.c_str());
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--csv") return csvMode(argv[2]);
    if (argc != 2) { std::fprintf(stderr, "usage: %s STREAM | --csv ROWS\n", argv[0]); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    bool bounded = true;
    auto whole = run(data, data.empty() ? 1 : data.size(), bounded);
    auto bytewise = run(data, 1, bounded);

    if (whole != bytewise) { std::fprintf(stderr, "chunking changed the result\n"); return 3; }
    if (!bounded) { std::fprintf(stderr, "a cap was exceeded\n"); return 4; }
    for (const auto &line : whole) std::printf("%s\n", line.c_str());
    return 0;
}
