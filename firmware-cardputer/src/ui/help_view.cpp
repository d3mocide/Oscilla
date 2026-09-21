/*
 * help_view.cpp — the shared Cardputer keyboard reference.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/help_view.h"

#include "ui/canvas.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

void drawHelpView(const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 3);
    d.setTextColor(kFieldGreen, kVoidInk);
    d.print("QUICK CONTROLS");

    d.setCursor(4, kBodyTop + 18);
    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.print(", /   CARDS");
    d.setCursor(4, kBodyTop + 31);
    d.print("; .   ROWS");
    d.setCursor(4, kBodyTop + 44);
    d.print("ENTER SELECT / ACT");
    d.setCursor(4, kBodyTop + 57);
    d.print("`     BACK/STOP, SETTINGS ON LINK");
    d.setCursor(4, kBodyTop + 70);
    d.print("s/c   START / STREAM");
    d.setCursor(4, kDetailRowY);
    d.print("h     CLOSE HELP");

    endChrome(chrome);
}

}  // namespace ui
