// Simulated preview for keyboards that cannot report their LEDs (standard VIA firmware): QMK's own effects worked out
// in the app from the effect, speed and colour the keyboard reports through VIA, the key layout and key presses.
// The maths is QMK's (quantum/rgb_matrix/animations, master f5db962, GPL-2.0-or-later); effects that use random
// numbers (raindrops, starlight, pixel rain...) are close copies, not exact.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "layout.h"

namespace sim {
// The keys to light (in the preview's order), the keymap's first layer (for modifier keys and key presses)
void set_keys(const std::vector<KeyRect> &keys, const std::vector<uint16_t> &keymap, int cols);
// ZXKB keyboards: the keyboard's own LEDs (x, y, flags each, as it reports them: QMK's 224 x 64 space), and the PC
// scan code of the key over each LED (0 = none); colours then come one per LED
void set_leds(const std::vector<uint8_t> &xyflags, const std::vector<uint16_t> &scan);
bool known(const std::string &effect);                     // the simulator has this effect
void key_hit(uint16_t scancode);                           // a key press (PC scan code set 1, 0xE0xx extended)
// Colours for every key (r, g, b each) at time ms; false if the effect is not simulated
bool render(const std::string &effect, uint8_t h, uint8_t s, uint8_t v, uint8_t speed, uint32_t ms, uint8_t *rgb);
// The keys' own colours and effects (ZXKB firmware's regular layer, zxkb.c): 8 bytes per LED (proto::KEY_BYTES),
// scaled by brightness v (0-255); keys that follow the effect stay dark, as on the keyboard
void keys_frame(const uint8_t *key, int leds, uint8_t v, uint32_t ms, uint8_t *rgb);
void hsv(uint8_t h, uint8_t s, uint8_t v, uint8_t *rgb); // QMK's hsv_to_rgb
} // namespace sim
