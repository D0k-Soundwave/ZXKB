// The ZXKB window: opened by double-clicking the tray icon, next to the tray; fully freed when closed.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void window_show();          // open it next to the tray icon, or bring it to the front
void window_refresh();       // the keyboard or the menu changed something: show it
void window_close();
HWND window_handle();
// While keys are selected for colouring, the keyboard shows the key colours (the regular layer) instead of the effect
// over them (D0k, 2026-11-06); back to the effect when nothing is selected or the window closes. Not saved.
bool edit_view_showing();
void edit_view_effect_chosen(); // an effect was chosen: show it, until the keys are selected again        // for the message loop (keyboard navigation); null when closed
