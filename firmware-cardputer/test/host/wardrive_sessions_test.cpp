/* Host regression tests for exact wardrive filenames and bounded index choice. */

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>

#include "storage/wardrive_sessions.h"

int main()
{
    int index = 0;
    bool is_kml = false;

    assert(storage::sessions::parse("drive_0001.csv", &index, &is_kml));
    assert(index == 1 && !is_kml);
    assert(storage::sessions::parse("drive_9999.kml", &index, &is_kml));
    assert(index == 9999 && is_kml);

    const char *bad[] = {
        "drive_0000.csv", "drive_10000.csv", "drive_0001.txt",
        "drive_001.csv", "drive_0001.csv.bak", "drive_000a.kml", "xdrive_0001.kml"
    };
    for (const char *name : bad) assert(!storage::sessions::parse(name, &index, &is_kml));
    assert(!storage::sessions::parse("drive_0001.csv", nullptr, &is_kml));

    bool used[10000] = {};
    assert(storage::sessions::firstFree(used, 10000) == 1);
    used[1] = true;
    used[2] = true;
    assert(storage::sessions::firstFree(used, 10000) == 3);
    used[3] = true;
    assert(storage::sessions::firstFree(used, 10000) == 4);
    used[9999] = true;
    for (int i = 4; i < 9999; ++i) used[i] = true;
    assert(storage::sessions::firstFree(used, 10000) == 0);
    assert(storage::sessions::firstFree(nullptr, 10000) == 0);

    std::puts("wardrive sessions test OK");
    return 0;
}
