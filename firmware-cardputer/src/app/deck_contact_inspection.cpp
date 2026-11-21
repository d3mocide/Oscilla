/* Sniffer identity anchoring across authoritative snapshot replacements. */
#include "app/deck_app.h"
namespace app {
void DeckApp::openContactInspection()
{
    const bool probes=contacts_tab_==ui::ContactsTab::Probes;
    const auto key=model::contactKey(contacts_,probes,contacts_cursor_);
    const model::ApRow *ap=nullptr;
    if(!probes)for(const auto &row:scan_.rows())if(row.bssid==key.target){ap=&row;break;}
    if(!contact_inspection_.open(contacts_,probes,contacts_cursor_,ap))notice("no contact selected");
    dirty_=true;
}
void DeckApp::absorbContactSnapshot(const ocp::Item &item)
{
    const bool probes=contacts_tab_==ui::ContactsTab::Probes;
    const auto key=contact_inspection_.active()?contact_inspection_.key():model::contactKey(contacts_,probes,contacts_cursor_);
    if(item.tag==OCP_MARK_CLIENTS)contacts_.absorbClients(item,now_);
    else contacts_.absorbProbes(item,now_);
    contacts_cursor_=model::contactCursor(contacts_,key,contacts_cursor_);
    contact_inspection_.refresh(contacts_);
}
}
