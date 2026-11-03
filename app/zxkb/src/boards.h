// What ZXKB knows about keyboards that do not describe themselves (plain VIA firmware): their public VIA definition,
// looked up online once by USB ID (only when the user agrees) and kept in %APPDATA%\ZXKB\boards.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include "hidlink.h"

namespace boards {
bool         has_definition(const Board &b);           // a VIA definition is kept for this keyboard
bool         apply_definition(Board *b);               // effect names and lighting channel from the kept definition
bool         lookup_online(uint16_t vid, uint16_t pid, std::wstring *error); // download and keep it (blocking)
std::wstring definition_source();                      // where lookups go, for the question to the user
} // namespace boards
