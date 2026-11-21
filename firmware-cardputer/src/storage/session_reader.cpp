/* Read-only snapshots; TFT/SD operations share storage::lock (Rev D §5). */
#include "storage/session_reader.h"
#include "storage/sd_storage.h"
#include <Arduino.h>
#include <SD.h>
namespace storage {
namespace {
enum class Job { Idle, Catalog, Open, Page, Cancel };
Job pending = Job::Idle;
File directory, file;
model::SessionParser parser;
int folder = 0;
unsigned entries = 0;
bool listing = false, reading = false, page_only = false;
uint32_t last_progress = 0;
size_t wanted_page = 0;
const char *path(model::SessionKind kind) {
    return kind == model::SessionKind::Drive ? "/oscilla/wardrive" : "/oscilla/lora";
}
void close() {
    if (directory) directory.close();
    if (file) file.close();
    listing = reading = false;
}
void fail(model::SessionReview &r, const char *text) {
    r.error = text;
    r.busy = false;
    pending = Job::Cancel;
}
void queue(model::SessionReview &r, Job job) {
    pending = job;
    r.error.clear();
    r.busy = true;
    last_progress = millis();
}
} // namespace
void sessionCatalog(model::SessionReview &r) {
    queue(r, Job::Catalog);
    r.stage = model::ReviewStage::Catalog;
}
void sessionOpen(model::SessionReview &r) {
    if (r.cursor >= r.catalog.entries.size()) return;
    r.selected = r.catalog.entries[r.cursor];
    r.summary = {};
    r.rows.clear();
    r.record = r.page_index = 0;
    r.packet.close();
    r.stage = model::ReviewStage::Summary;
    queue(r, Job::Open);
}
void sessionPage(model::SessionReview &r, size_t page) {
    wanted_page = page;
    queue(r, Job::Page);
}
void sessionCancel(model::SessionReview &r) {
    if(r.busy){
        if(r.stage==model::ReviewStage::Catalog)r.limited=true;
        else r.error="REVIEW CANCELLED / REOPEN";
    }
    pending = Job::Cancel;
    r.busy = false;
}
void sessionTick(model::SessionReview &r, uint32_t now) {
    if (pending == Job::Idle && !listing && !reading) return;
    if (now - last_progress > 5000 && r.busy) {
        fail(r, "SD BUS TIMEOUT");
    }
    if (!lock(0)) return;
    if (pending != Job::Idle) {
        const auto job = pending;
        pending = Job::Idle;
        close();
        if (job == Job::Cancel) {
            unlock();
            return;
        }
        r.available = ready();
        if (!r.available) {
            unlock();
            fail(r, "SD UNAVAILABLE");
            return;
        }
        if (job == Job::Catalog) {
            r.catalog = {};
            r.cursor = 0;
            r.limited = false;
            folder = 0;
            entries = 0;
            listing = true;
        } else {
            if (!(r.selected.parts & model::Csv)) {
                unlock();
                fail(r, "CSV MISSING / SIDECARS ONLY");
                return;
            }
            const auto name =
                std::string(path(r.selected.kind)) + "/" + model::sessionStem(r.selected) + ".csv";
            file = SD.open(name.c_str(), FILE_READ);
            if (!file || file.isDirectory()) {
                unlock();
                fail(r, "FILE UNAVAILABLE");
                return;
            }
            if (job == Job::Open)
                r.snapshot_bytes = file.size();
            else if (file.size() < r.snapshot_bytes) {
                unlock();
                fail(r, "FILE SHRANK / REFRESH");
                return;
            }
            r.page_index = job == Job::Page ? wanted_page : 0;
            r.record = 0;
            r.read_bytes = 0;
            r.rows.clear();
            parser.begin(r.selected.kind, r.page_index);
            reading = true;
            page_only = job == Job::Page;
        }
        last_progress = now;
        unlock();
        return;
    }
    if (listing) {
        if (!directory) {
            if ((r.filter == 1 && folder == 1) || (r.filter == 2 && folder == 0)) {
                if (++folder == 2) {
                    listing = false;
                    r.busy = false;
                }
                unlock();
                return;
            }
            directory = SD.open(folder == 0 ? "/oscilla/wardrive" : "/oscilla/lora", FILE_READ);
            if (!directory || !directory.isDirectory()) {
                if (directory) directory.close();
                if (++folder == 2) {
                    listing = false;
                    r.busy = false;
                }
            }
            last_progress = now;
            unlock();
            return;
        }
        File entry = directory.openNextFile();
        if (!entry) {
            directory.close();
            if (++folder == 2) {
                listing = false;
                r.busy = false;
            }
            last_progress = now;
            unlock();
            return;
        }
        const std::string raw = entry.name();
        const auto slash = raw.find_last_of('/');
        const auto name = slash == std::string::npos ? raw : raw.substr(slash + 1);
        model::SessionEntry row;
        const bool accepted =
            !entry.isDirectory() &&
            model::sessionName(name, folder == 0 ? model::SessionKind::Drive : model::SessionKind::Lora, row);
        row.bytes = entry.size();
        entry.close();
        ++entries;
        last_progress = now;
        if (entries >= 4096) {
            directory.close();
            listing = false;
            r.busy = false;
            r.limited = true;
        }
        unlock();
        if (accepted) r.catalog.add(row);
        return;
    }
    if (reading) {
        char buffer[512];
        const auto remaining = r.snapshot_bytes - r.read_bytes;
        int n = remaining ? file.read(reinterpret_cast<uint8_t *>(buffer),
                                      std::min<uint32_t>(sizeof buffer, remaining))
                          : 0;
        if (remaining && n <= 0) {
            file.close();
            reading = false;
            unlock();
            fail(r, "READ FAILED / REFRESH");
            return;
        }
        r.read_bytes += n;
        const bool done = r.read_bytes == r.snapshot_bytes;
        if (done) {
            file.close();
            reading = false;
        }
        last_progress = now;
        unlock();
        for (int i = 0; i < n; ++i) parser.feed(buffer[i]);
        if (page_only && parser.page.size() == 6 && !done) {
            if (lock(0)) {
                file.close();
                reading = false;
                unlock();
            }
        }
        if (done || !reading) {
            parser.finish();
            if (!page_only) r.summary = parser.summary;
            r.rows = parser.page;
            r.busy = false;
            if (!r.summary.header) r.error = "UNRECOGNIZED CSV HEADER";
        }
        return;
    }
    unlock();
}
} // namespace storage
