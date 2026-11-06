// Tests for the preview's drawing of the keys' own colours (sim::keys_frame), against the firmware's rules (zxkb.c).
// Build: g++ -std=c++17 -DUNICODE -D_UNICODE -o sim_keys_test.exe test/sim_keys_test.cpp src/sim.cpp src/labels.cpp -luser32
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/sim.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace proto {
int key_step_ms(int speed) { return (int)(5000.0 * std::pow(16.0 / 5000.0, speed / 255.0) + 0.5); } // as main.cpp
}

int g_fail = 0;
void check(bool ok, const char *what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_fail++;
}

int main() {
    uint8_t key[3 * 8] = {
        1, 122, 200, 0, 200, 0, 0, 0,     // steady magenta
        2, 122, 255, 0, 0, 0, 0, 255,     // flash red / blue, 320 ms a step
        0, 0, 9, 9, 9, 9, 9, 9,           // follows the effect: dark
    };
    uint8_t rgb[9];
    sim::keys_frame(key, 3, 255, 0, rgb);
    check(rgb[0] == 200 && rgb[1] == 0 && rgb[2] == 200, "steady key shows its first colour");
    check(rgb[3] == 255 && rgb[5] == 0, "flash: first colour in the first step");
    check(rgb[6] == 0 && rgb[7] == 0 && rgb[8] == 0, "a key that follows the effect is dark when the effect is off");
    int step = proto::key_step_ms(122);
    sim::keys_frame(key, 3, 255, (uint32_t)step, rgb);
    check(rgb[3] == 0 && rgb[5] == 255, "flash: second colour in the second step");
    sim::keys_frame(key, 3, 128, 0, rgb);
    check(rgb[0] == 200 * 128 / 255 && rgb[2] == 200 * 128 / 255, "brightness scales the key colours as the firmware");
    std::vector<uint8_t> leds = {0, 0, 1, 224, 64, 4};
    sim::set_leds(leds, {0, 0});
    uint8_t two[6];
    check(sim::render("SOLID_COLOR", 0, 255, 255, 128, 0, two) && two[0] == 255 && two[3] == 255, "QMK effects draw on the keyboard's own LEDs");
    check(sim::render("ALPHAS_MODS", 0, 255, 255, 85, 0, two) && two[0] != two[3], "LED flags mark the modifier keys");
    std::printf("%s\n", g_fail ? "FAILED" : "all passed");
    return g_fail != 0;
}
