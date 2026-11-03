// Physical key layouts in KLE form (keyboard-layout-editor.com), as VIA definitions and keycap sets carry them.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>

struct Json;

struct KeyRect {
    int x = 0, y = 0, w = 100, h = 100; // hundredths of a key
    int x2 = 0, y2 = 0, w2 = 0, h2 = 0; // second rectangle (ISO Enter, big-ass Enter); w2 = 0 if none
    int row = -1, col = -1;             // keyboard matrix position ("row,col" in VIA files), -1 if not given
    bool knob = false;                  // rotary encoder (VIA: "e" in label 9)
    std::vector<std::string> labels;    // KLE labels, top-left first (12 positions, missing ones empty)
};

namespace kle {
// Keys from a KLE array of rows. VIA layout options: only keys with no option or choice 0 are kept.
std::vector<KeyRect> from_kle(const Json &rows);
// Keys from a VIA definition's "layouts": raw KLE ("keymap", as in VIA's source files) or VIA's compiled form
// ("keys" with row, col, x, y, w, h, x2/y2/w2/h2, "ei" for knobs; "optionKeys" alternatives, choice 0 kept)
std::vector<KeyRect> from_via(const Json &layouts);
void bounds(const std::vector<KeyRect> &keys, int *w, int *h); // size in hundredths of a key
} // namespace kle
