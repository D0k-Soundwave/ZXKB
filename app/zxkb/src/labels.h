// Key labels from what each key does, in the user's Windows keyboard language: no keycap database needed.
// QMK key codes -> USB HID usage -> scan code -> the character Windows types for it in the chosen layout.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace labels {
struct Label {
    std::wstring main; // the key's character or name ("Q", "1", "Enter", "Fn")
    std::wstring sub;  // the character with Shift, where it differs and is not just a capital ("!" on 1)
};
// custom: names of the keyboard's own key codes (QK_KB + i), from its VIA definition
Label for_keycode(uint16_t kc, HKL layout, const std::vector<std::wstring> &custom);
HKL   current_layout();                 // the keyboard layout in use now
std::vector<HKL> installed();          // the layouts installed in Windows
std::wstring layout_name(HKL layout);  // Windows' own name for the layout ("United Kingdom", "US - Spectrum ...")
} // namespace labels
