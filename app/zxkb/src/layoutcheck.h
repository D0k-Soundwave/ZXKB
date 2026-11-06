// Key layout backups and the check after a firmware update (D0k, 2026-11-06): before installing firmware the whole
// layout (every layer, the knob, the macros) is copied from the keyboard to a file; after the keyboard restarts it is
// read again and compared. Each difference is listed with "yours" and "new firmware" side by side, ticked to put
// yours back where ZXKB knows you changed it (and for macros), else left for the new firmware. After putting back,
// the layout is read once more and checked. Backups stay in %APPDATA%\ZXKB\backups and can be put back any time.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace layoutcheck {
bool possible(); // the keyboard's keymap can be read (VIA, matrix known)
bool read_keymap(std::vector<uint16_t> *keys, int *layers); // every layer, layer * rows * cols + row * cols + col
// Copy the whole layout to a new backup file; returns its path, empty if it could not be read
std::wstring backup(const std::wstring &why, std::wstring *error);
// Compare a backup with the keyboard and show the differences (modal); after_update: the firmware was just installed
void check(HWND owner, const std::wstring &file, bool after_update);
void choose_and_check(HWND owner); // pick a backup file, then check it
std::wstring folder();
} // namespace layoutcheck
