// Macros played by ZXKB instead of the keyboard (D0k, 2026-11-06: as much as possible from the app, any keyboard):
// once, repeat while held, or on / off, with waits that vary ({wait 200-300}). The macro's keys on the keyboard are
// given a spare key code, F13-F24, which the input hook catches (press and release) and blocks; ZXKB then types the
// macro itself. Only while ZXKB runs. 12 codes in all, less those Fn + 1..0 use on a standard keyboard (F13-F22).
// Kept per keyboard in its keystore file ([appmacros] M<n>=<F-key 0-11>,<mode>).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace appmacros {
enum Mode { KEYBOARD = -1, ONCE = 0, HELD = 1, TOGGLE = 2 };
Mode mode(int macro);                              // who plays it, and how
bool set_mode(int macro, Mode m, std::wstring *error); // moves its keys to / from the spare code
uint16_t trigger_code(int macro);                  // the QMK key code its keys carry (0 if the keyboard plays it)
int  free_codes();                                 // spare codes left
void reload();                                     // tray: which codes play which macros (after a change)
void key(int f, bool down);                        // tray: F13 + f pressed or let go (from the input hook)
uint16_t hook_mask();                              // bit f: F13 + f is a macro's code
void stop_all();
} // namespace appmacros
