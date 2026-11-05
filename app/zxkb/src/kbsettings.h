// The Keyboard settings window: game mode, SOCD, debounce, lights off when idle, the layer light, NKRO and macro
// play modes (ZXKB firmware v27 on, command 0xDF), and Windows' own key repeat delay and rate (any keyboard).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void kbsettings_dialog(HWND owner); // not modal; changes show live, OK keeps them, Cancel puts back what was there
HWND kbsettings_window();
bool kbsettings_possible(); // the keyboard has the settings (ZXKB protocol 12, firmware v27)
