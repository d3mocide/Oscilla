/* Bounded offline session snapshots; hostile SD text is validated here. */
#pragma once
#include "model/inspection_state.h"
#include <array>
#include <vector>
namespace model {
enum class SessionKind { Drive, Lora };
enum SessionPart { Csv = 1, Kml = 2, Manifest = 4, Health = 8 };
struct SessionEntry {
    SessionKind kind = SessionKind::Drive;
    int index = 0;
    unsigned parts = 0;
    uint64_t bytes = 0;
};
bool sessionName(const std::string &name, SessionKind directory, SessionEntry &entry);
std::string sessionStem(const SessionEntry &entry);
struct SessionCatalog {
    std::vector<SessionEntry> entries;
    bool overflow = false;
    void add(const SessionEntry &entry);
};
struct SavedRecord {
    std::string name, identity, auth, stamp;
    int rssi = 0, ch = 0;
    uint32_t frequency = 0;
    int sf = 0, bw = 0, cr = 0;
    LoraPacket packet;
    uint64_t time = 0;
    bool timed = false;
};
struct SessionSummary {
    bool header = false, partial = false, time_order = true;
    uint32_t valid = 0, bad = 0;
    uint64_t first = 0, last = 0;
    uint32_t timed = 0;
    std::string first_stamp, last_stamp;
    std::array<int, 32> signals{};
    size_t signal_count = 0;
    uint64_t spanMs() const { return timed == valid && timed && time_order ? last - first : 0; }
    bool hasSpan() const { return timed == valid && timed && time_order; }
    void add(const SavedRecord &record);
};
class SessionParser {
    SessionKind kind_ = SessionKind::Drive;
    size_t page_index_ = 0;
    unsigned line_number_ = 0;
    std::string line_;
    bool overlong_ = false;
    void line();

  public:
    SessionSummary summary;
    std::vector<SavedRecord> page;
    void begin(SessionKind kind, size_t page_index = 0);
    void feed(char byte);
    void finish();
};
enum class ReviewStage { Catalog, Summary, Records, Hex };
struct SessionReview {
    SessionCatalog catalog;
    unsigned filter = 0;
    size_t cursor = 0, record = 0, page_index = 0;
    SessionEntry selected;
    SessionSummary summary;
    std::vector<SavedRecord> rows;
    PacketInspection packet;
    ReviewStage stage = ReviewStage::Catalog;
    bool busy = false, available = false, limited = false;
    uint32_t snapshot_bytes = 0, read_bytes = 0;
    std::string error;
    void back() {
        if (stage == ReviewStage::Hex) {
            packet.close();
            stage = ReviewStage::Records;
        } else if (stage == ReviewStage::Records)
            stage = ReviewStage::Summary;
        else if (stage == ReviewStage::Summary)
            stage = ReviewStage::Catalog;
    }
};
} // namespace model
