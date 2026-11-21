/*
 * baseline_store.cpp — see baseline_store.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/baseline_store.h"

#include <SD.h>

#include "storage/sd_activity.h"
#include "storage/sd_storage.h"

namespace storage {

namespace {
constexpr const char *kDir = "/oscilla/baselines";
constexpr const char *kDefaultPath = "/oscilla/baselines/default.csv";
constexpr size_t kMaxBaselineBytes = 32768;
}  // namespace

bool saveBaseline(const model::BaselineWatch &watch)
{
    if (!ready() || !watch.active()) return false;
    const std::string payload = watch.serializeCsv();
    if (!lock()) return false;
    SD.mkdir("/oscilla");
    SD.mkdir(kDir);
    File f = SD.open(kDefaultPath, FILE_WRITE);
    bool ok = false;
    if (f) {
        const size_t wrote = f.print(payload.c_str());
        sdActivity().record(payload.size(), wrote, millis());
        f.close();
        ok = wrote == payload.size();
    }
    unlock();
    return ok;
}

bool loadBaseline(model::BaselineWatch &watch)
{
    if (!ready()) return false;
    if (!lock()) return false;
    std::string payload;
    if (SD.exists(kDefaultPath)) {
        File f = SD.open(kDefaultPath, FILE_READ);
        if (f && !f.isDirectory()) {
            const size_t sz = f.size();
            const size_t cap = sz < kMaxBaselineBytes ? sz : kMaxBaselineBytes;
            payload.resize(cap);
            if (cap > 0) {
                const size_t got = f.read(reinterpret_cast<uint8_t *>(&payload[0]), cap);
                payload.resize(got);
            }
            f.close();
        } else if (f) {
            f.close();
        }
    }
    unlock();
    if (payload.empty()) return false;
    return watch.parseCsv(payload);
}

bool clearSavedBaseline()
{
    if (!ready()) return false;
    if (!lock()) return false;
    const bool ok = !SD.exists(kDefaultPath) || SD.remove(kDefaultPath);
    unlock();
    return ok;
}

}  // namespace storage
