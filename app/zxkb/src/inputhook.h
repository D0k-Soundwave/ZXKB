// Keyboard functions done by ZXKB itself, for any keyboard (D0k, 2026-11-06: everything from the app, whatever firmware
// the keyboard runs): game mode (Alt+Tab, Alt+F4, Windows keys blocked), SOCD (of two opposite keys held, the last
// pressed counts) and the time of the last key press (for lights off when idle). A low-level keyboard hook on its own
// thread, so a busy tray never delays typing; it runs only while one of these is switched on. Works while ZXKB runs.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>

namespace inputhook {
struct Config {
    bool    game  = false; // game mode on
    uint8_t block = 7;     // bit 0 Alt+Tab, 1 Alt+F4, 2 Windows keys (none = all three, as the firmware)
    uint8_t socd  = 0;     // bit 0 A/D and W/S, bit 1 arrows
    bool    watch = false; // note key presses (lights off when idle)
};
void  configure(const Config &c);     // starts or stops the hook as needed
DWORD last_key();                     // GetTickCount() of the last key press seen (0 if none yet)
void  notify_next_key(HWND w, UINT msg); // post msg to w at the next key press (once), e.g. to put the lights back
} // namespace inputhook
