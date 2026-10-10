#include <cassert>
#include <cstdio>
#include "storage/write_digest.h"

int main()
{
    const uint8_t data[] = {0, 1, 255, '\n', 42};
    storage::WriteDigest whole, chunks, changed, truncated;
    whole.append(data, sizeof(data));
    chunks.append(data, 2);
    chunks.append(data + 2, sizeof(data) - 2);
    assert(whole.matches(chunks));
    uint8_t hostile[] = {0, 1, 254, '\n', 42};
    changed.append(hostile, sizeof(hostile));
    truncated.append(data, sizeof(data) - 1);
    assert(!whole.matches(changed));
    assert(!whole.matches(truncated));
    assert(!whole.matches(storage::WriteDigest{}));
    std::puts("Write digest: chunk invariant, changed/truncated/empty readback rejected");
}
