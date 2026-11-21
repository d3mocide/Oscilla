/* Saved-session navigation; browsing never claims a radio lane. */
#include "app/deck_app.h"
#include "storage/session_reader.h"
namespace app {
void DeckApp::openSessions(uint32_t now) {
    navigateCard(Screen::Logs, now);
    if (session_review_.catalog.entries.empty() && !session_review_.busy)
        storage::sessionCatalog(session_review_);
}
void DeckApp::sessionKeys(char c) {
    auto &r = session_review_;
    if (c == 'r' && r.stage == model::ReviewStage::Catalog) {
        storage::sessionCatalog(r);
        return;
    }
    if (c == 'x' && r.stage == model::ReviewStage::Catalog) {
        r.filter = (r.filter + 1) % 3;
        storage::sessionCatalog(r);
        return;
    }
    if (r.busy) return;
    if (r.stage == model::ReviewStage::Hex) {
        if (c == ';' || c == '.') r.packet.page(c == ';' ? -1 : 1);
        return;
    }
    if (r.stage == model::ReviewStage::Catalog) {
        if (c == ';' && r.cursor)
            --r.cursor;
        else if (c == '.' && r.cursor + 1 < r.catalog.entries.size())
            ++r.cursor;
    } else if (r.stage == model::ReviewStage::Records) {
        if (c == ';') {
            if (r.record)
                --r.record;
            else if (r.page_index) {
                session_previous_page_ = true;
                storage::sessionPage(r, r.page_index - 1);
            }
        } else if (c == '.') {
            if (r.record + 1 < r.rows.size())
                ++r.record;
            else if ((r.page_index + 1) * 6 < r.summary.valid) {
                session_previous_page_ = false;
                storage::sessionPage(r, r.page_index + 1);
            }
        }
    }
}
void DeckApp::sessionEnter() {
    auto &r = session_review_;
    if (r.busy) return;
    dirty_=true;
    if (r.stage == model::ReviewStage::Catalog)
        storage::sessionOpen(r);
    else if (r.stage == model::ReviewStage::Summary && r.error.empty() && r.summary.valid) {
        r.stage = model::ReviewStage::Records;
        r.record = 0;
    } else if (r.stage == model::ReviewStage::Records && r.selected.kind == model::SessionKind::Lora &&
               r.record < r.rows.size()) {
        r.packet.open(r.rows[r.record].packet);
        if (r.packet.active()) r.stage = model::ReviewStage::Hex;
    }
}
void DeckApp::tickSessions(uint32_t now) {
    const bool busy = session_review_.busy;
    storage::sessionTick(session_review_, now);
    if (busy && !session_review_.busy) {
        if (session_previous_page_ && !session_review_.rows.empty())
            session_review_.record = session_review_.rows.size() - 1;
        session_previous_page_ = false;
        dirty_ = true;
    }
}
} // namespace app
