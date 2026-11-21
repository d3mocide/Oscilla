/* Sequential fields avoid simultaneous formatter temporaries on the loop stack. */
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
namespace debug {
class DiagnosticLine {
    std::string line_;
    void key(const char *name) {
        line_ += ' ';
        line_ += name;
        line_ += '=';
    }

  public:
    explicit DiagnosticLine(const char *prefix) : line_(prefix) { line_.reserve(2048); }
    const std::string &str() const { return line_; }
    std::string take() { return std::move(line_); }
    DiagnosticLine &field(const char *name, const std::string &value) {
        key(name);
        for (unsigned char c : value)
            line_ += c > 32 && c < 127 ? char(c) : '?';
        return *this;
    }
    template <class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    DiagnosticLine &field(const char *name, T value) {
        char text[32];
        if constexpr (std::is_signed<T>::value)
            std::snprintf(text, sizeof text, "%lld", static_cast<long long>(value));
        else
            std::snprintf(text, sizeof text, "%llu", static_cast<unsigned long long>(value));
        key(name);
        line_ += text;
        return *this;
    }
};
} // namespace debug
