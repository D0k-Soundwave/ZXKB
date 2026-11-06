// Profiles by program (D0k's step 7, any keyboard): when a chosen program comes to the front, ZXKB switches to its
// profile; when another program comes to the front, the lighting from before comes back. Windows reports each change
// of the front window (SetWinEventHook, EVENT_SYSTEM_FOREGROUND), so nothing is polled. Tray copy only.
// While a program's profile is in use nothing is saved to the keyboard: unplugged or with ZXKB closed, the keyboard
// starts with the lighting from before. Programs and profiles: zxkb.ini [apps], "game.exe=Profile name".
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <string>

namespace apps {
void start();   // tray: listen for the front program (when any program has a profile), and look at it now
void stop();
bool active();  // a program's profile is in use (lighting not saved)
void forget();  // the keyboard was disconnected: it starts again with its own saved lighting
void check_now(); // look at the front program again (keyboard connected, list changed)
std::wstring profile_for(const std::wstring &exe); // "" if none
void front_changed(const std::wstring &exe);       // the front program is now exe (lower case, "game.exe")
std::wstring exe_of(HWND w);                        // lower-case file name of a window's program, "" if unknown
void dialog(HWND owner);                            // the Profiles by program window (appsdlg.cpp)
} // namespace apps
