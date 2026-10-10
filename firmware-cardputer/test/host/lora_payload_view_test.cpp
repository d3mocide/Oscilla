#include "model/lora_payload_view.h"
#include <cassert>
#include <cstdio>
int main() {
    const auto row = model::payloadLine("001b0d0a7f804125",0);
    assert(row.bytes == 8 && row.hex == "00 1B 0D 0A 7F 80 41 25");
    assert(row.ascii == "......A%");
    const auto tail = model::payloadLine("000102030405060708",8);
    assert(tail.bytes == 1 && tail.hex == "08" && tail.ascii == ".");
    assert(model::payloadLine("0Z",0).bytes == 0);
    assert(model::payloadLine("123",0).bytes == 0);
    assert(model::payloadLine("ab",static_cast<size_t>(-1)).bytes == 0);
    assert(model::payloadLine("",0).bytes == 0);
    puts("LoRa payload display: controls, non-ASCII, bounds and malformed hex passed");
}
