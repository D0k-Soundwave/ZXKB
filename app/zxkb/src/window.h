// The ZXKB window: opened by double-clicking the tray icon, next to the tray; fully freed when closed.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void window_show();          // open it next to the tray icon, or bring it to the front
void window_refresh();       // the keyboard or the menu changed something: show it
void window_close();
HWND window_handle();        // for the message loop (keyboard navigation); null when closed
