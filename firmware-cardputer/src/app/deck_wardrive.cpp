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
void DeckApp::finishWardrive()
{
    const auto &s=storage::wardriveLogStats();
    log("wardrive log end aps="+std::to_string(s.aps)+" nofix="+std::to_string(s.aps_no_fix)+" trk="+std::to_string(s.track_points));
    storage::wardriveLogEnd();
    wardrive_closing_=false;wardrive_rescan_=false;
    next_page_=0;scan_pending_=false;scan_.stop();notice("log closed");
}
void DeckApp::tickWardrive(uint32_t now)
{
    if(!storage::wardriveLogStats().open){wardrive_closing_=false;wardrive_rescan_=false;return;}
    logScanRows();
    if(wardriveRowsPending())return;
    if(wardrive_closing_){finishWardrive();return;}
    if(wardrive_rescan_ && !client_.pending() && !next_page_)requestScan(now);
}
}
