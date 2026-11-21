/* Host-only read-only filesystem with lock/budget assertions. */
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
inline constexpr const char *FILE_READ = "r";
extern bool session_test_locked;
extern size_t session_test_read_bytes, session_test_entries;
struct SessionTestNode {
    std::string name, data;
    bool directory = false;
    std::vector<std::shared_ptr<SessionTestNode>> children;
};
class File {
    std::shared_ptr<SessionTestNode> node_;
    size_t position_ = 0;

  public:
    File() = default;
    explicit File(std::shared_ptr<SessionTestNode> n) : node_(std::move(n)) {}
    explicit operator bool() const { return bool(node_); }
    void close() {
        assert(session_test_locked);
        node_.reset();
        position_ = 0;
    }
    bool isDirectory() const {
        assert(session_test_locked);
        return node_ && node_->directory;
    }
    const char *name() const {
        assert(session_test_locked);
        return node_ ? node_->name.c_str() : "";
    }
    uint32_t size() const {
        assert(session_test_locked);
        return node_ ? node_->data.size() : 0;
    }
    int read(uint8_t *buffer, size_t length) {
        assert(session_test_locked && node_);
        assert(!node_->directory);
        assert(length <= 512);
        const size_t n = std::min(length, node_->data.size() - std::min(position_, node_->data.size()));
        std::memcpy(buffer, node_->data.data() + position_, n);
        position_ += n;
        session_test_read_bytes += n;
        return n;
    }
    File openNextFile() {
        assert(session_test_locked && node_ && node_->directory);
        ++session_test_entries;
        return position_ < node_->children.size() ? File(node_->children[position_++]) : File();
    }
};
struct SessionTestSD {
    std::map<std::string, std::shared_ptr<SessionTestNode>> nodes;
    File open(const char *path, const char *mode) {
        assert(session_test_locked && std::string(mode) == "r");
        auto i = nodes.find(path);
        return i == nodes.end() ? File() : File(i->second);
    }
};
extern SessionTestSD SD;
