// Key functions for remapping. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
// [P] QMK key code values: quantum/keycodes.h (master f5db962): basic 0x0000-0x00FF (USB HID usages), QK_TO 0x5200,
// QK_MOMENTARY 0x5220, QK_DEF_LAYER 0x5240, QK_TOGGLE_LAYER 0x5260, QK_ONE_SHOT_LAYER 0x5280, QK_MACRO 0x7700,
// QK_RGB_MATRIX_ON 0x7840.., QK_MAGIC_TOGGLE_GUI 0x700B, QK_MAGIC_TOGGLE_NKRO 0x7013, QK_BOOTLOADER 0x7C00,
// QK_KB 0x7E00, QK_COMMUNITY_MODULE 0x77C0 (ZXKB: ZK_1..ZK_0 0x77C0-0x77C9, ZK_VU 0x77CA, ZK_GAME 0x77CB, ZK_MREC 0x77CC).
#include "keycodes.h"
#include "labels.h"
#include <cwchar>

namespace keycodes {
namespace {
enum Group { LETTERS, NUMBERS, SYMBOLS, FKEYS, EDITING, KEYPAD, MODS, MEDIA, MOUSE, LAYERS, LIGHTING, ZXKB, MACROS, SPECIAL, GROUPS };

struct Named { uint16_t code; const wchar_t *name; };
const Named media[] = {
    {0xA8, L"Mute"}, {0xA9, L"Volume up"}, {0xAA, L"Volume down"}, {0xAB, L"Next track"}, {0xAC, L"Previous track"},
    {0xAD, L"Stop"}, {0xAE, L"Play / pause"}, {0xBD, L"Screen brighter"}, {0xBE, L"Screen dimmer"}, {0xB2, L"Calculator"},
    {0xB3, L"File Explorer"}, {0xB5, L"Browser home"}, {0xB1, L"Mail"}, {0xA6, L"Sleep"},
};
const Named mouse[] = {
    {0xCD, L"Mouse up"}, {0xCE, L"Mouse down"}, {0xCF, L"Mouse left"}, {0xD0, L"Mouse right"}, {0xD1, L"Left click"},
    {0xD2, L"Right click"}, {0xD3, L"Middle click"}, {0xD9, L"Wheel up"}, {0xDA, L"Wheel down"},
};
const Named lighting[] = {
    {0x7842, L"Lighting on / off"}, {0x7843, L"Next effect"}, {0x7844, L"Previous effect"}, {0x7849, L"Brighter"},
    {0x784A, L"Dimmer"}, {0x784B, L"Faster"}, {0x784C, L"Slower"}, {0x7845, L"Hue +"}, {0x7846, L"Hue -"},
    {0x7847, L"Saturation +"}, {0x7848, L"Saturation -"},
};
const Named special[] = {
    {0x0000, L"Nothing (key does nothing)"}, {0x0001, L"Same as the layer below"}, {0x7013, L"NKRO on / off"},
    {0x700B, L"Windows key lock on / off"}, {0x7C00, L"Bootloader (firmware update)"},
};

std::wstring basic_name(uint8_t usage, HKL layout) {
    labels::Label l = labels::for_keycode(usage, layout, {});
    if (l.main.empty()) return L"";
    return l.sub.empty() ? l.main : l.main + L"  " + l.sub;
}
} // namespace

const std::vector<std::wstring> &group_names() {
    static const std::vector<std::wstring> g = {L"Letters", L"Numbers", L"Symbols", L"Function keys", L"Editing and moving",
                                                L"Keypad", L"Modifiers", L"Media and system", L"Mouse", L"Layers",
                                                L"Lighting", L"ZXKB", L"Macros (stored on the keyboard)", L"Special"};
    return g;
}

std::vector<Choice> choices(HKL layout, int layers, int macros, const std::vector<std::wstring> &custom, bool zxkb) {
    std::vector<Choice> v;
    auto add = [&](uint16_t c, const std::wstring &n, int g) { if (!n.empty()) v.push_back({c, n, g}); };
    for (uint8_t u = 0x04; u <= 0x1D; u++) add(u, basic_name(u, layout), LETTERS);
    for (uint8_t u = 0x1E; u <= 0x27; u++) add(u, basic_name(u, layout), NUMBERS);
    for (uint8_t u : {0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x64}) add(u, basic_name(u, layout), SYMBOLS);
    for (uint8_t u = 0x3A; u <= 0x45; u++) add(u, basic_name(u, layout), FKEYS);  // F1-F12
    for (uint8_t u = 0x68; u <= 0x73; u++) add(u, basic_name(u, layout), FKEYS);  // F13-F24
    for (uint8_t u : {0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x39, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x65}) {
        std::wstring n = basic_name(u, layout);
        add(u, u == 0x2C ? L"Space" : n, EDITING);
    }
    for (uint8_t u = 0x53; u <= 0x63; u++) add(u, L"Keypad " + basic_name(u, layout), KEYPAD);
    const wchar_t *mods[8] = {L"Left Ctrl", L"Left Shift", L"Left Alt", L"Left Windows", L"Right Ctrl", L"Right Shift", L"Right Alt (AltGr)", L"Right Windows"};
    for (int i = 0; i < 8; i++) add((uint16_t)(0xE0 + i), mods[i], MODS);
    for (const Named &m : media) add(m.code, m.name, MEDIA);
    for (const Named &m : mouse) add(m.code, m.name, MOUSE);
    for (int l = 1; l < layers && l < 16; l++) {
        add((uint16_t)(0x5220 + l), L"Hold for layer " + std::to_wstring(l) + L" (Fn)", LAYERS);
        add((uint16_t)(0x5260 + l), L"Switch layer " + std::to_wstring(l) + L" on / off", LAYERS);
        add((uint16_t)(0x5280 + l), L"Layer " + std::to_wstring(l) + L" for the next key only", LAYERS);
    }
    for (int l = 0; l < layers && l < 16; l++) add((uint16_t)(0x5200 + l), L"Go to layer " + std::to_wstring(l), LAYERS);
    for (const Named &m : lighting) add(m.code, m.name, LIGHTING);
    if (zxkb) {
        for (int i = 0; i < 10; i++) add((uint16_t)(0x77C0 + i), L"ZXKB hotkey " + std::to_wstring((i + 1) % 10), ZXKB);
        add(0x77CA, L"VU meter on / off", ZXKB);
        add(0x77CB, L"Game mode on / off", ZXKB); // firmware v27 on
        add(0x77CC, L"Record a macro (start / stop)", ZXKB); // firmware v29 on
    }
    for (int m = 0; m < macros && m < 128; m++) add((uint16_t)(0x7700 + m), L"Macro " + std::to_wstring(m), MACROS);
    for (size_t i = 0; i < custom.size() && i < 64; i++) add((uint16_t)(0x7E00 + i), custom[i] + L" (keyboard's own)", SPECIAL);
    for (const Named &m : special) add(m.code, m.name, SPECIAL);
    return v;
}

std::wstring describe(uint16_t code, HKL layout, const std::vector<std::wstring> &custom) {
    for (const Choice &c : choices(layout, 16, 128, custom, true))
        if (c.code == code) return c.name;
    if (code > 0xFF && code <= 0x1FFF) { // a key with modifiers held
        std::wstring m;
        if (code & 0x100) m += L"Ctrl+";
        if (code & 0x200) m += L"Shift+";
        if (code & 0x400) m += L"Alt+";
        if (code & 0x800) m += L"Win+";
        return (code & 0x1000 ? L"Right " : L"") + m + basic_name((uint8_t)code, layout);
    }
    if (code >= 0x4000 && code <= 0x4FFF) return basic_name((uint8_t)code, layout) + L", held: layer " + std::to_wstring((code >> 8) & 15);
    if (code >= 0x2000 && code <= 0x3FFF) return basic_name((uint8_t)code, layout) + L", held: a modifier";
    wchar_t s[24];
    swprintf(s, 24, L"key code 0x%04X", code);
    return s;
}

} // namespace keycodes
