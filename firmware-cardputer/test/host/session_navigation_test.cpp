#include "app/deck_app.h"
#include "storage/session_reader.h"
#include <cassert>
#include <iostream>
namespace storage {
void sessionCatalog(model::SessionReview &r){r.busy=true;r.stage=model::ReviewStage::Catalog;}
void sessionOpen(model::SessionReview &r){r.busy=true;r.stage=model::ReviewStage::Summary;}
void sessionPage(model::SessionReview &r,size_t p){r.busy=true;r.page_index=p;r.record=0;}
void sessionTick(model::SessionReview &r,uint32_t){r.busy=false;}
}
int main(){
 app::DeckApp app;auto &r=app.session_review_;r.stage=model::ReviewStage::Summary;r.summary.valid=8;
 model::SavedRecord row;row.packet.len=1;row.packet.hex="41";r.rows.assign(6,row);r.selected.kind=model::SessionKind::Lora;
 app.sessionEnter();assert(r.stage==model::ReviewStage::Records&&app.dirty_);
 app.dirty_=false;app.sessionEnter();assert(r.stage==model::ReviewStage::Hex&&app.dirty_&&r.packet.active());
 r.back();r.record=5;app.sessionKeys('.');assert(r.busy&&r.page_index==1);app.tickSessions(100);assert(!r.busy&&app.dirty_);
 app.sessionKeys(';');assert(r.busy&&r.page_index==0);app.tickSessions(200);assert(r.record==5);
 r.stage=model::ReviewStage::Catalog;r.filter=0;app.sessionKeys('x');assert(r.filter==1&&r.busy);
 app.dirty_=false;r.stage=model::ReviewStage::Summary;app.sessionEnter();assert(!app.dirty_&&r.stage==model::ReviewStage::Summary);
 std::cout<<"session navigation: offline redraw, summary/hex entry, cross-page selection, type filter and busy guard passed\n";
}
