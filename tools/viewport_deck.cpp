/* Preview wiring for actual onboard view/chrome source. */
#include "ui/canvas.h"
namespace ui {
M5Canvas &canvas(){static M5Canvas d;return d;}
void initCanvas(){canvas().createSprite(240,135);}
void present(){}
}
