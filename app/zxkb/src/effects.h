// Effect names as the menu shows them, and which submenu each goes in.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

namespace effects {
enum Group { SPECTRUM, SOLID, RAINBOW, RAIN, TYPING, OTHER, HIDDEN, GROUP_COUNT };
extern const wchar_t *const group_names[GROUP_COUNT];

// firmware name (e.g. "SPECTRUM_STRIPES") -> menu name ("Spectrum stripes"); unknown names are tidied up
std::wstring friendly(const std::string &name);
Group        group(const std::string &name, int kind);
} // namespace effects
