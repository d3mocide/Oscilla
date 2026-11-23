/* One AP write per loop; page fixes retain their receipt-time provenance. */
#include "app/deck_app.h"
#include "storage/wardrive_logger.h"
namespace app {
bool DeckApp::wardriveRowsPending()const
{
    return storage::wardriveLogStats().open && wardrive_logged_upto_<scan_.rows().size();
}
void DeckApp::logScanRows()
{
    if(!storage::wardriveLogStats().open)return;
    const auto &rows=scan_.rows();
    if(wardrive_logged_upto_>rows.size())wardrive_logged_upto_=0;
    if(wardrive_logged_upto_==rows.size())return;
    storage::wardriveLogAp(rows[wardrive_logged_upto_],wardrive_page_fix_,now_-wardrive_page_fix_ms_);
    ++wardrive_logged_upto_;
}
bool DeckApp::wardrivePaused() const
{
    return storage::wardriveLogStats().open && !wardrive_closing_ && wardrive_stream_supported_ &&
           !wardrive_stream_active_ && !wardrive_stream_pending_;
}
/* Something holds the PHY lane: restarts the settle wait. */
const char *DeckApp::wardrivePhyBlocker() const
{
    if (client_.state() != ocp::LinkState::Ready) return "link";
    if (phy_handoff_.queued()) return "handoff";
    if (phyEngineActive()) return "engine";
    if (phyToolActive()) return "tool_pending";
    if (hunt_.active()) return "hunt";
    if (!probe_owner_.empty() && probe_owner_ != OCP_OWNER_NONE) return "owner";
    return "";
}
/* Momentarily busy (status poll, paging, draining): only defers the send. */
const char *DeckApp::wardriveSendBlocker() const
{
    if (client_.pending()) return "client";
    if (client_.stopPending()) return "stop";
    if (next_page_) return "page";
    if (pending_retry_) return "retry";
    if (wardriveRowsPending()) return "rows";
    return "";
}
const char *DeckApp::wardriveResumeBlocker() const
{
    const char *phy = wardrivePhyBlocker();
    return *phy ? phy : wardriveSendBlocker();
}
void DeckApp::tickWardriveResume(uint32_t now_ms)
{
    if (!wardrive_resume_.due(wardrivePaused(), *wardrivePhyBlocker() == '\0',
                              *wardriveSendBlocker() == '\0', now_ms)) return;
    wardrive_resume_.reset();
    log("wardrive resume");
    wardrive_auto_start_ = true;
    startWardriveStream(now_ms);
}
void DeckApp::startWardriveStream(uint32_t now_ms)
{
    if (!storage::wardriveLogStats().open || wardrive_closing_) return;
    if (client_.state() != ocp::LinkState::Ready) return;
    if (!client_.send(OCP_V_START_WARDRIVE, now_ms)) {
        retrySoon([this](uint32_t t) { startWardriveStream(t); }, now_ms);
        return;
    }
    wardrive_stream_pending_ = true;
}
void DeckApp::enqueueWardriveEvent(const ocp::Item &it)
{
    if (!storage::wardriveLogStats().open || !wardrive_stream_active_ || wardrive_closing_) return;
    const auto *kind = it.get(OCP_K_KIND);
    if (!kind) return;
    model::GnssFix fix = gnss_.fix();
    const uint32_t age = gnss_.fixAgeMs(now_);
    fix.valid = gnss_.state(now_) == model::GnssState::Fixed && age < 3000;

    if (*kind == OCP_EVT_KIND_NETWORK) {
        const auto *bssid = it.get(OCP_K_BSSID);
        if (!bssid || wardrive_ap_queue_.size() >= kMaxWardriveQueue) return;
        const size_t idx = model::apIndex(scan_, *bssid);
        if (idx < scan_.rows().size()) {
            wardrive_ap_queue_.push_back({scan_.rows()[idx], fix, age});
        }
    } else if (*kind == OCP_EVT_KIND_BLE) {
        const auto *mac = it.get(OCP_K_MAC);
        if (!mac || wardrive_bt_queue_.size() >= kMaxWardriveQueue) return;
        const size_t idx = model::btIndex(bt_, *mac);
        if (idx < bt_.devices().size()) {
            wardrive_bt_queue_.push_back({bt_.devices()[idx], fix, age});
        }
    }
}
bool DeckApp::drainWardriveStream()
{
    if (!storage::wardriveLogStats().open) {
        wardrive_ap_queue_.clear();
        wardrive_bt_queue_.clear();
        return false;
    }
    if (!wardrive_ap_queue_.empty()) {
        const auto item = std::move(wardrive_ap_queue_.front());
        wardrive_ap_queue_.pop_front();
        storage::wardriveLogAp(item.ap, item.fix, item.fix_age_ms);
        return true;
    }
    if (!wardrive_bt_queue_.empty()) {
        const auto item = std::move(wardrive_bt_queue_.front());
        wardrive_bt_queue_.pop_front();
        storage::wardriveLogBt(item.dev, item.fix, item.fix_age_ms);
        return true;
    }
    return false;
}
void DeckApp::finishWardrive()
{
    const auto &s=storage::wardriveLogStats();
    log("wardrive log end aps="+std::to_string(s.aps)+" nofix="+std::to_string(s.aps_no_fix)+
        " bt="+std::to_string(s.bt)+" bt_nofix="+std::to_string(s.bt_no_fix)+
        " trk="+std::to_string(s.track_points));
    storage::wardriveLogEnd();
    wardrive_ap_queue_.clear();wardrive_bt_queue_.clear();
    wardrive_stream_active_=false;wardrive_stream_pending_=false;
    wardrive_closing_=false;wardrive_rescan_=false;
    next_page_=0;scan_pending_=false;scan_.stop();bt_.stop();notice("log closed");
}
void DeckApp::tickWardrive(uint32_t now)
{
    if(!storage::wardriveLogStats().open){wardrive_closing_=false;wardrive_rescan_=false;return;}
    /* start_wardrive owns AP rows (fresh fix per event); page logging is the legacy fallback only. */
    if(wardrive_stream_supported_)wardrive_logged_upto_=scan_.rows().size();
    logScanRows();
    if(wardriveRowsPending())return;
    if(wardrive_closing_){finishWardrive();return;}
    if(wardrive_rescan_ && !client_.pending() && !next_page_)requestScan(now);
}
}
