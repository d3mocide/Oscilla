#include "debug/diagnostic_line.h"
#include <cassert>
#include <iostream>
int main() {
    debug::DiagnosticLine line("dump");
    line.field("count", UINT32_MAX)
        .field("signed", -200)
        .field("active", true)
        .field("state", std::string("ready\n bad\033"));
    assert(line.str() == "dump count=4294967295 signed=-200 active=1 state=ready??bad?");
    for (unsigned i = 0; i < 200; ++i)
        line.field("value", i);
    assert(line.str().find('\n') == std::string::npos && line.str().find("value=199") != std::string::npos);
    const auto expected = line.str();
    assert(line.take() == expected);
    std::cout << "diagnostic line: sequential fields, numeric bounds and single-line text passed\n";
}
