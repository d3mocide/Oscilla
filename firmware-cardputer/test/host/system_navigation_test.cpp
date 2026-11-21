#include "app/system_navigation.h"
#include <cassert>
#include <iostream>
int main(){
    app::SystemNavigation n;
    n.enter(app::Screen::Spectrum);assert(n.active()&&n.origin()==app::Screen::Spectrum);
    assert(n.preserves(app::Screen::Info)&&n.preserves(app::Screen::Link)&&n.preserves(app::Screen::Spectrum));
    assert(!n.preserves(app::Screen::Beacons));
    n.enter(app::Screen::Link);assert(n.origin()==app::Screen::Spectrum);
    n.clear();assert(!n.active()&&!n.preserves(app::Screen::Spectrum));
    n.enter(app::Screen::SubGhz);assert(n.origin()==app::Screen::SubGhz);
    for(auto source:{app::Screen::Sweep,app::Screen::Beacons,app::Screen::Spectrum,app::Screen::Contacts,
                     app::Screen::Zig,app::Screen::SubGhz,app::Screen::Trace,app::Screen::Drive}){
        n.clear();n.enter(source);assert(n.active()&&n.origin()==source);
        assert(n.preserves(app::Screen::Info)&&n.preserves(app::Screen::Link)&&n.preserves(source));
        assert(n.preserves(app::Screen::Logs));
        n.enter(app::Screen::Logs);assert(n.origin()==source);
        n.enter(app::Screen::Info);assert(n.origin()==source);
        n.clear();assert(!n.active());
    }
    std::cout<<"system navigation: preserves receiver, origin, nested system cards and exit ownership passed\n";
}
