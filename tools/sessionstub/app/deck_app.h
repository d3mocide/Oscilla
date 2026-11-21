#pragma once
#include "app/deck_navigation.h"
#include "model/session_review.h"
namespace app {
class DeckApp {
public:
 model::SessionReview session_review_;bool session_previous_page_=false,dirty_=false;Screen screen_=Screen::Logs;
 void navigateCard(Screen target,uint32_t){screen_=target;}
 void openSessions(uint32_t);void sessionKeys(char);void sessionEnter();void tickSessions(uint32_t);
};
}
