/* System inspection preserves its originating tool and receiver. */
#pragma once
#include "app/deck_navigation.h"
namespace app {
inline bool isSystemScreen(Screen s){return s==Screen::Info||s==Screen::Link;}
inline bool isInspectionScreen(Screen s){return isSystemScreen(s)||s==Screen::Logs;}
class SystemNavigation {
    bool active_=false;Screen origin_=Screen::Link;
public:
    void enter(Screen from){if(!isInspectionScreen(from)){origin_=from;active_=true;}}
    bool active()const{return active_;}
    Screen origin()const{return origin_;}
    bool preserves(Screen target)const{return isInspectionScreen(target)||(active_&&target==origin_);}
    void clear(){active_=false;}
};
}
