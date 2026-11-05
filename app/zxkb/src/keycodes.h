// What a key can be set to: QMK key codes by group, named for people (letters in the Windows keyboard layout).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace keycodes {
struct Choice {
    uint16_t     code;
    std::wstring name;
    int          group; // index into group_names()
};
const std::vector<std::wstring> &group_names();
// Everything a key can do on this keyboard: layers (count), macros (count), the keyboard's own codes (custom names),
// ZXKB's own keys (zxkb: a ZXKB keyboard)
std::vector<Choice> choices(HKL layout, int layers, int macros, const std::vector<std::wstring> &custom, bool zxkb);
std::wstring describe(uint16_t code, HKL layout, const std::vector<std::wstring> &custom); // "Q", "Hold for layer 1", "0x1234"
} // namespace keycodes
