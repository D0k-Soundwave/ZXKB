// Fn + number hotkeys for plain VIA keyboards. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fnkeys.h"
#include "app.h"
#include "settings.h"
#include <cwchar>
#include <cwchar>
#include <cstdlib>
#include <vector>

namespace fnkeys {
namespace {

// [P] VIA: id_dynamic_keymap_get_keycode 0x04 [layer, row, col] -> [.., hi, lo]; set 0x05 [layer, row, col, hi, lo]
// (QMK quantum/via.h). QMK key codes (quantum/keycodes.h): KC_1-KC_0 0x1E-0x27, KC_F13 0x68, MO(n) 0x5220 + n.
constexpr uint8_t VIA_GET_KEY = 0x04, VIA_SET_KEY = 0x05;
constexpr uint16_t KC_1 = 0x1E, KC_F13 = 0x68, MO_FIRST = 0x5220, MO_LAST = 0x523F;

std::wstring id() {
    wchar_t s[16];
    swprintf(s, 16, L"%04X_%04X", g_board ? g_board->vid : 0, g_board ? g_board->pid : 0);
    return s;
}

struct Saved { int layer, row, col; uint16_t old; };

// "layer,row,col,old;..." in zxkb.ini [fnkeys] <USB ID>
std::vector<Saved> load() {
    std::vector<Saved> v;
    std::wstring s = settings::get(L"fnkeys", id().c_str());
    for (size_t p = 0; p < s.size();) {
        size_t e = s.find(L';', p);
        std::wstring part = s.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
        Saved k;
        wchar_t *e1, *e2, *e3, *e4;
        k.layer = (int)wcstol(part.c_str(), &e1, 10);
        k.row   = (int)wcstol(e1 + (*e1 == L','), &e2, 10);
        k.col   = (int)wcstol(e2 + (*e2 == L','), &e3, 10);
        k.old   = (uint16_t)wcstoul(e3 + (*e3 == L','), &e4, 16);
        if (*e1 == L',' && *e2 == L',' && *e3 == L',' && e4 > e3 + 1) v.push_back(k);
        if (e == std::wstring::npos) break;
        p = e + 1;
    }
    return v;
}

bool get_key(int layer, int row, int col, uint16_t *kc) {
    uint8_t q[4] = {VIA_GET_KEY, (uint8_t)layer, (uint8_t)row, (uint8_t)col}, r[32];
    if (!keyboard_ask(q, 4, r, 4)) return false;
    *kc = (uint16_t)(r[4] << 8 | r[5]);
    return true;
}

bool set_key(int layer, int row, int col, uint16_t kc) {
    uint8_t q[6] = {VIA_SET_KEY, (uint8_t)layer, (uint8_t)row, (uint8_t)col, (uint8_t)(kc >> 8), (uint8_t)kc}, r[32];
    return keyboard_ask(q, 6, r, 4);
}

} // namespace

bool possible() { return g_board && g_board->kind == Board::VIA && g_board->via_protocol && g_board->rows && g_board->cols; }

bool active() { return possible() && !load().empty(); }

bool set_up(std::wstring *message) {
    if (!possible()) { *message = L"This keyboard's keymap cannot be read (its layout is not known yet)."; return false; }
    if (active()) { *message = L"Fn + 1 to Fn + 0 are already set up on this keyboard."; return true; }
    if (!read_keymap_now()) { *message = L"The keyboard's keymap could not be read."; return false; }
    const std::vector<uint16_t> &km = g_board->keymap;
    int cols = g_board->cols, fn = -1;
    int pos[10];
    for (int &p : pos) p = -1;
    for (size_t i = 0; i < km.size(); i++) {
        if (km[i] >= MO_FIRST && km[i] <= MO_LAST && fn < 0) fn = km[i] - MO_FIRST;
        if (km[i] >= KC_1 && km[i] < KC_1 + 10 && pos[km[i] - KC_1] < 0) pos[km[i] - KC_1] = (int)i;
    }
    if (fn < 1) { *message = L"No Fn key (a key that holds another layer) was found on this keyboard's first layer."; return false; }
    std::wstring saved;
    int done = 0;
    for (int n = 0; n < 10; n++) { // KC_1 .. KC_0 -> slots 1 .. 10
        if (pos[n] < 0) continue;
        int row = pos[n] / cols, col = pos[n] % cols;
        uint16_t old;
        if (!get_key(fn, row, col, &old)) continue;
        if (!set_key(fn, row, col, (uint16_t)(KC_F13 + n))) continue;
        wchar_t part[48];
        swprintf(part, 48, L"%d,%d,%d,%04X;", fn, row, col, old);
        saved += part;
        done++;
    }
    if (!done) { *message = L"The keyboard did not take the changes."; return false; }
    settings::set(L"fnkeys", id().c_str(), saved); // kept first, so Undo can always put the keys back
    listen(g_hwnd);
    *message = L"Fn + 1 to Fn + 0 now send F13 to F22, which ZXKB answers like the ZXKB firmware's hotkeys (" +
               std::to_wstring(done) + L" keys set on layer " + std::to_wstring(fn) + L").";
    return true;
}

bool undo(std::wstring *message) {
    std::vector<Saved> v = load();
    int put = 0;
    for (const Saved &k : v) put += set_key(k.layer, k.row, k.col, k.old);
    stop_listening(g_hwnd);
    if (put < (int)v.size()) {
        *message = L"Only " + std::to_wstring(put) + L" of " + std::to_wstring(v.size()) + L" keys could be put back; try again.";
        return false;
    }
    settings::set(L"fnkeys", id().c_str(), L"");
    *message = L"The keyboard's own keys are back on Fn + 1 to Fn + 0.";
    return true;
}

void listen(HWND hwnd) {
    stop_listening(hwnd);
    if (!active()) return;
    for (int i = 0; i < 10; i++) RegisterHotKey(hwnd, HOTKEY_BASE + i, MOD_NOREPEAT, VK_F13 + i);
}

void stop_listening(HWND hwnd) {
    for (int i = 0; i < 10; i++) UnregisterHotKey(hwnd, HOTKEY_BASE + i);
}

} // namespace fnkeys
