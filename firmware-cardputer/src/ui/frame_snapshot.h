/* Display copies own RAM only until their view is left (DESIGN §7.1). */
#pragma once
#include <new>

namespace ui::detail {
template<class T>
void discardFrameCopy(T &snapshot) {
    snapshot.~T();
    new (&snapshot) T{};
}
}
