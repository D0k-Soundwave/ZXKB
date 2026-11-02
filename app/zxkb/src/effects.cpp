// Effect names as the menu shows them, and which submenu each goes in.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "effects.h"
#include "settings.h"
#include <cstring>

namespace effects {

const wchar_t *const group_names[GROUP_COUNT] = {
    L"Spectrum", L"Solid and gradients", L"Rainbows and cycles", L"Rain and stars", L"Reacts to typing", L"Other", L"",
};

namespace {
struct Known {
    const char    *name;
    const wchar_t *menu;
    Group          group;
};
const Known known[] = {
    // ZXKB module
    {"SPECTRUM_STRIPES", L"Spectrum stripes", SPECTRUM},
    {"TAPE_LOADING", L"Tape loading", SPECTRUM},
    {"BORDER_FLASH", L"Border flash", SPECTRUM},
    {"ATTR_CLASH", L"Attribute clash", SPECTRUM},
    {"TYPING_RIPPLE", L"Typing ripple", SPECTRUM},
    {"LOGO_SWEEP", L"Logo sweep", SPECTRUM},
    {"KEY_EXPLOSION", L"Key explosion", TYPING},
    {"VU_METER", L"VU meter", HIDDEN},   // driven by the VU meter switch, not picked as an effect
    {"PC_DIRECT", L"PC direct", HIDDEN}, // driven by the PC
    // QMK
    {"SOLID_COLOR", L"Solid colour", SOLID},
    {"ALPHAS_MODS", L"Letters and modifiers", SOLID},
    {"GRADIENT_UP_DOWN", L"Gradient top to bottom", SOLID},
    {"GRADIENT_LEFT_RIGHT", L"Gradient left to right", SOLID},
    {"BREATHING", L"Breathing", SOLID},
    {"BAND_SAT", L"Band (saturation)", SOLID},
    {"BAND_VAL", L"Band (brightness)", SOLID},
    {"BAND_PINWHEEL_SAT", L"Pinwheel band (saturation)", SOLID},
    {"BAND_PINWHEEL_VAL", L"Pinwheel band (brightness)", SOLID},
    {"BAND_SPIRAL_SAT", L"Spiral band (saturation)", SOLID},
    {"BAND_SPIRAL_VAL", L"Spiral band (brightness)", SOLID},
    {"CYCLE_ALL", L"Cycle all", RAINBOW},
    {"CYCLE_LEFT_RIGHT", L"Cycle left to right", RAINBOW},
    {"CYCLE_UP_DOWN", L"Cycle top to bottom", RAINBOW},
    {"RAINBOW_MOVING_CHEVRON", L"Rainbow chevron", RAINBOW},
    {"CYCLE_OUT_IN", L"Cycle out to in", RAINBOW},
    {"CYCLE_OUT_IN_DUAL", L"Cycle out to in (dual)", RAINBOW},
    {"CYCLE_PINWHEEL", L"Cycle pinwheel", RAINBOW},
    {"CYCLE_SPIRAL", L"Cycle spiral", RAINBOW},
    {"DUAL_BEACON", L"Dual beacon", RAINBOW},
    {"RAINBOW_BEACON", L"Rainbow beacon", RAINBOW},
    {"RAINBOW_PINWHEELS", L"Rainbow pinwheels", RAINBOW},
    {"FLOWER_BLOOMING", L"Flower blooming", RAINBOW},
    {"HUE_BREATHING", L"Hue breathing", RAINBOW},
    {"HUE_PENDULUM", L"Hue pendulum", RAINBOW},
    {"HUE_WAVE", L"Hue wave", RAINBOW},
    {"RIVERFLOW", L"Riverflow", RAINBOW},
    {"FLOW", L"Flow", RAINBOW},
    {"RAINDROPS", L"Raindrops", RAIN},
    {"JELLYBEAN_RAINDROPS", L"Jellybean raindrops", RAIN},
    {"PIXEL_RAIN", L"Pixel rain", RAIN},
    {"PIXEL_FLOW", L"Pixel flow", RAIN},
    {"PIXEL_FRACTAL", L"Pixel fractal", RAIN},
    {"DIGITAL_RAIN", L"Digital rain", RAIN},
    {"STARLIGHT", L"Starlight", RAIN},
    {"STARLIGHT_SMOOTH", L"Starlight smooth", RAIN},
    {"STARLIGHT_DUAL_HUE", L"Starlight dual hue", RAIN},
    {"STARLIGHT_DUAL_SAT", L"Starlight dual saturation", RAIN},
    {"TYPING_HEATMAP", L"Typing heatmap", TYPING},
    {"SOLID_REACTIVE_SIMPLE", L"Reactive", TYPING},
    {"SOLID_REACTIVE", L"Reactive (hue shift)", TYPING},
    {"SOLID_REACTIVE_WIDE", L"Reactive wide", TYPING},
    {"SOLID_REACTIVE_MULTIWIDE", L"Reactive wide (multi)", TYPING},
    {"SOLID_REACTIVE_CROSS", L"Reactive cross", TYPING},
    {"SOLID_REACTIVE_MULTICROSS", L"Reactive cross (multi)", TYPING},
    {"SOLID_REACTIVE_NEXUS", L"Reactive nexus", TYPING},
    {"SOLID_REACTIVE_MULTINEXUS", L"Reactive nexus (multi)", TYPING},
    {"SPLASH", L"Splash", TYPING},
    {"MULTISPLASH", L"Splash (multi)", TYPING},
    {"SOLID_SPLASH", L"Solid splash", TYPING},
    {"SOLID_MULTISPLASH", L"Solid splash (multi)", TYPING},
};

const Known *find(const std::string &name) {
    for (const Known &k : known)
        if (name == k.name) return &k;
    return nullptr;
}
} // namespace

std::wstring friendly(const std::string &name) {
    if (const Known *k = find(name)) return k->menu;
    std::wstring w = settings::widen(name); // "MY_EFFECT" -> "My effect"
    for (size_t i = 0; i < w.size(); i++) {
        if (w[i] == L'_') w[i] = L' ';
        else if (i > 0) w[i] = (wchar_t)towlower(w[i]);
    }
    return w;
}

Group group(const std::string &name, int kind) {
    if (const Known *k = find(name)) return k->group;
    return kind == 0xFF ? HIDDEN : OTHER;
}

} // namespace effects
