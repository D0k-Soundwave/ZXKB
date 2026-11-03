// Keycap sets: an optional extra over the default labels (what each key does, in the Windows keyboard language).
// A set is a KLE file (keyboard-layout-editor.com) with legends and colours; ZXKB ships D0k's KAM Wraith set and the
// user can import others. Sets live in <ZXKB.exe folder>\keycaps and %APPDATA%\ZXKB\keycaps; the choice is kept per
// keyboard (USB ID).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "layout.h"

namespace keycaps {
std::vector<std::wstring> list();                                     // set names (file names without .json), sorted
bool         load(const std::wstring &name, std::vector<KeyRect> *keys);
std::wstring chosen(uint16_t vid, uint16_t pid);                       // "" = labels from the keyboard language
void         choose(uint16_t vid, uint16_t pid, const std::wstring &name);
bool         import(const std::wstring &path, std::wstring *name, std::wstring *error); // copy a KLE file in
// The set key that covers a keyboard key: same matrix position if the set has them, else the nearest key centre
const KeyRect *match(const std::vector<KeyRect> &set, const KeyRect &key, const std::vector<KeyRect> &board);
} // namespace keycaps
