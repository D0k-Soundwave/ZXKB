// The key changes made through ZXKB, kept per keyboard (%APPDATA%\ZXKB\keymaps\<USB ID>.ini), so they can be put
// back after a firmware update: VIA resets the keymap to the firmware's own whenever a different firmware build starts
// (QMK quantum/via.c: its settings check is the build date). Only the keys changed here are put back, so new
// defaults in the new firmware stay.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace keystore {
void record(int layer, int row, int col, uint16_t code); // a key set through ZXKB
void forget(int layer, int row, int col);                // put back to the firmware's own (no longer ZXKB's to keep)
void forget_all();
int  count();                                            // key changes kept for the connected keyboard
int  reapply(int *failed);                               // set every kept change on the keyboard; returns how many changed
bool firmware_changed();                                 // this keyboard's firmware differs from last time (then noted)
} // namespace keystore
