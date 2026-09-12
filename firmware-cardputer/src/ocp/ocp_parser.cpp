/*
 * ocp_parser.cpp — see ocp_parser.h. Each step mirrors tools/ocp.py.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_parser.h"

#include <string_view>
#include <utility>
#include <vector>

#include "ocp_text.h"

namespace ocp {

const std::string *Item::get(const char *key) const
{
    for (const auto &e : kv) {
        if (e.key == key) return &e.value;
    }
    return nullptr;
}

namespace {

using std::string_view;

bool isSpace(char c) { return c == ' ' || c == '\t'; }   /* OCP-SPEC §2: SP/HT only */

string_view stripSpaces(string_view s)
{
    size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b])) ++b;
    while (e > b && isSpace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

bool isMarker(string_view t)
{
    if (t.size() < 3 || t.front() != '[' || t.back() != ']') return false;
    for (size_t i = 1; i + 1 < t.size(); ++i) {
        char c = t[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool isKey(string_view k)
{
    if (k.empty()) return false;
    for (char c : k) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
    }
    return true;
}

/* Tokens are contiguous runs; a quoted run keeps its spaces. False on an
 * unterminated quote. */
bool tokenize(string_view s, std::vector<string_view> &out)
{
    size_t i = 0, n = s.size(), start = string_view::npos;
    bool in_quote = false;

    while (i < n) {
        char c = s[i];
        if (in_quote) {
            if (c == '\\' && i + 1 < n) { i += 2; continue; }
            if (c == '"') in_quote = false;
            ++i;
            continue;
        }
        if (c == '"') {
            if (start == string_view::npos) start = i;
            in_quote = true;
            ++i;
            continue;
        }
        if (isSpace(c)) {
            if (start != string_view::npos) {
                out.push_back(s.substr(start, i - start));
                start = string_view::npos;
            }
            ++i;
            continue;
        }
        if (start == string_view::npos) start = i;
        ++i;
    }
    if (in_quote) return false;
    if (start != string_view::npos) out.push_back(s.substr(start));
    return true;
}

/* False on a malformed escape, before anything is committed to `out`. */
bool parseKv(const std::vector<string_view> &toks, size_t from, size_t to,
             std::vector<KeyValue> &out)
{
    std::vector<KeyValue> kv;
    uint8_t buf[OCP_MAX_LINE_LEN];

    for (size_t i = from; i < to; ++i) {
        string_view tok = toks[i];
        size_t eq = tok.find('=');
        if (eq == string_view::npos) continue;
        string_view key = tok.substr(0, eq);
        if (!isKey(key)) continue;
        string_view val = tok.substr(eq + 1);

        std::string decoded;
        if (!val.empty() && val.front() == '"') {
            int n = ocp_unescape_field(val.data(), val.size(), buf, sizeof buf);
            if (n < 0) return false;
            decoded.assign(reinterpret_cast<const char *>(buf), (size_t)n);
        } else {
            decoded.assign(val.data(), val.size());
        }

        bool replaced = false;
        for (auto &e : kv) {
            if (e.key == key) { e.value = std::move(decoded); replaced = true; break; }
        }
        if (!replaced) kv.push_back({std::string(key), std::move(decoded)});
    }
    out = std::move(kv);
    return true;
}

Item noise(std::string text, const char *reason)
{
    Item it;
    it.kind = ItemKind::Noise;
    it.text = std::move(text);
    it.reason = reason;
    return it;
}

}  // namespace

Parser::Parser(size_t max_line_len, size_t max_frame_rows)
    : max_line_len_(max_line_len), max_frame_rows_(max_frame_rows)
{
    buf_.reserve(max_line_len_ + 1);
}

void Parser::feed(const uint8_t *data, size_t len, const Sink &sink)
{
    for (size_t i = 0; i < len; ++i) {
        char b = static_cast<char>(data[i]);
        if (b == '\n') {
            std::string line;
            line.swap(buf_);
            buf_.reserve(max_line_len_ + 1);
            bool was_overlong = overlong_;
            overlong_ = false;
            if (was_overlong) {
                sink(noise(std::move(line), "line exceeded OCP_MAX_LINE_LEN"));
            } else {
                feedLine(line, sink);
            }
            continue;
        }
        if (overlong_) continue;   /* discard to end of line */
        buf_.push_back(b);
        if (buf_.size() > max_line_len_) {
            overlong_ = true;
            buf_.resize(max_line_len_);
        }
    }
}

void Parser::feedLine(const std::string &raw, const Sink &sink)
{
    string_view line(raw);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.remove_suffix(1);
    string_view stripped = stripSpaces(line);
    if (stripped.empty()) return;

    std::vector<string_view> tokens;
    if (!tokenize(stripped, tokens)) {
        sink(noise(std::string(line), "unterminated quoted field"));
        return;
    }
    if (tokens.empty()) return;

    string_view tag = tokens[0];
    if (tag == OCP_REPLY_PONG && tokens.size() == 1) {
        Item it;
        it.kind = ItemKind::Pong;
        sink(std::move(it));
        return;
    }
    if (!isMarker(tag)) {
        sink(noise(std::string(line), "not a marker"));
        return;
    }

    size_t nrest = tokens.size() - 1;
    string_view raw_rest = stripSpaces(stripped.substr(tag.size()));
    bool is_begin = nrest > 0 && tokens[1] == OCP_KW_BEGIN;
    bool is_end = nrest > 0 && tokens.back() == OCP_KW_END;
    bool is_evt = tag == OCP_MARK_EVT;
    bool is_err = tag == OCP_MARK_ERR;

    /* Decode every k=v before touching state (OCP-SPEC §6). Rows stay raw. */
    size_t kv_from = 1, kv_to = 1;
    if (is_evt || is_err)  { kv_from = 1; kv_to = tokens.size(); }
    else if (is_begin)     { kv_from = 2; kv_to = tokens.size(); }
    else if (is_end)       { kv_from = 1; kv_to = tokens.size() - 1; }

    std::vector<KeyValue> kv;
    if (!parseKv(tokens, kv_from, kv_to, kv)) {
        sink(noise(std::string(line), "malformed escape"));
        return;
    }

    if (is_evt || is_err) {
        Item it;
        it.kind = is_evt ? ItemKind::Event : ItemKind::Error;
        it.tag = std::string(tag);
        it.kv = std::move(kv);
        sink(std::move(it));
        return;
    }

    /* [HELLO] outranks parser state: a probe that reset mid-frame (§4.1). */
    if (tag == OCP_MARK_HELLO && open_ && frame_.tag != OCP_MARK_HELLO) {
        open_ = false;
        sink(noise(frame_.tag + " BEGIN", "frame abandoned: probe reset mid-frame"));
        frame_ = Item();
    }

    if (open_ && tag != frame_.tag) {
        sink(noise(std::string(line), "tag inside open frame"));
        return;
    }

    if (is_begin) {
        if (open_) sink(noise(frame_.tag + " BEGIN", "frame re-opened without END"));
        frame_ = Item();
        frame_.kind = ItemKind::Frame;
        frame_.tag = std::string(tag);
        frame_.kv = std::move(kv);
        open_ = true;
        return;
    }

    if (is_end) {
        if (open_) {
            open_ = false;
            sink(std::move(frame_));
            frame_ = Item();
            return;
        }
        if (nrest == 1) {   /* late terminator, not an empty result (§3.2) */
            sink(noise(std::string(line), "END with no open frame"));
            return;
        }
        Item it;
        it.kind = ItemKind::Frame;
        it.tag = std::string(tag);
        it.compact = true;
        it.kv = std::move(kv);
        sink(std::move(it));
        return;
    }

    if (open_) {
        if (frame_.rows.size() >= max_frame_rows_) {   /* dropped whole (§1) */
            open_ = false;
            sink(noise(frame_.tag + " BEGIN", "frame exceeded OCP_MAX_FRAME_ROWS"));
            frame_ = Item();
            return;
        }
        frame_.rows.emplace_back(raw_rest);
        return;
    }

    sink(noise(std::string(line), "row outside any frame"));
}

bool Parser::abandonOpenFrame()
{
    bool was_open = open_;
    open_ = false;
    frame_ = Item();
    buf_.clear();
    overlong_ = false;
    return was_open;
}

}  // namespace ocp
