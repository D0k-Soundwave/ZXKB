// The key changes made through ZXKB, kept per keyboard (%APPDATA%\ZXKB\keymaps\<USB ID>.ini), so they can be put
// back after a firmware update: VIA resets the keymap to the firmware's own whenever a different firmware build starts
// (QMK quantum/via.c: its settings check is the build day, YY-MM-DD; builds on the same day keep the keymap). Only the keys changed here are put back, so new
// defaults in the new firmware stay.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace keystore {
void record(int layer, int row, int col, uint16_t code); // a key set through ZXKB
void forget(int layer, int row, int col);                // put back to the firmware's own (no longer ZXKB's to keep)
void record_knob(int layer, int encoder, bool cw, uint16_t code); // a knob direction set through ZXKB
void record_macros(const std::vector<std::wstring> &texts);        // the macros written through ZXKB
void forget_all();
int  count();                                            // key changes kept for the connected keyboard
int  reapply(int *failed);                               // set every kept change on the keyboard; returns how many changed
bool firmware_changed();                                 // this keyboard's firmware differs from last time (then noted)
// The keyboard's own macros become the ones kept, if they differ (a macro recorded on the keyboard, maybe without
// ZXKB running). Only when the firmware has not changed: after a change the keyboard's macros were reset.
void keep_keyboard_macros();
} // namespace keystore
