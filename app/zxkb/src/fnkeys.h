// Fn + number hotkeys on keyboards without the ZXKB firmware: with the user's OK, Fn + 1 to Fn + 0 are set to F13 to
// F22 in the keyboard's own keymap (through VIA; the keys that were there are kept, to put back), and ZXKB listens
// for F13 to F22 as Windows hotkeys (no polling). They work only while ZXKB runs.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <string>

namespace fnkeys {
bool possible();                    // a plain VIA keyboard whose keymap ZXKB can read and change
bool active();                      // set up on the connected keyboard
bool set_up(std::wstring *message); // change the keymap and start listening; message says what happened
bool undo(std::wstring *message);   // put the keyboard's own keys back and stop listening
void listen(HWND hwnd);             // register F13-F22 (after connecting), if set up for this keyboard
void stop_listening(HWND hwnd);     // unregister (on disconnect or quit)
constexpr int HOTKEY_BASE = 0x5A00; // WM_HOTKEY id = HOTKEY_BASE + slot - 1
} // namespace fnkeys
