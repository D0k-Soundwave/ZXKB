// Lock lights for any keyboard (D0k, 2026-11-06: in the app, not the firmware): while Caps Lock, Num Lock or Scroll
// Lock is on, ZXKB shows the whole keyboard in that lock's light (steady, flashing or breathing between two colours),
// then puts the keyboard's own lighting back. Stock VIA can only set the whole keyboard (effect, colour,
// brightness), not single keys; ZXKB firmware's Caps Lock light (caps.cpp) lights chosen keys. Done by the tray copy
// through VIA lighting commands that are never saved; works while ZXKB runs.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>

namespace locklights {
enum Which { CAPS = 0, NUM = 1, SCROLL = 2, COUNT = 3 }; // shown first to last when several are on
struct Light {
    uint8_t fx = 0;      // proto::KEY_NONE (off), KEY_STEADY, KEY_FLASH, KEY_BREATHE
    uint8_t speed = 122; // as the Caps Lock light: a flash step of proto::key_step_ms(speed); a breath is 5 steps
    uint8_t a[3] = {255, 0, 255}, b[3] = {0, 0, 0}; // colours (steady shows a)
    uint8_t full = 1;    // full brightness, whatever the brightness setting
};
const wchar_t *name(int which);       // "Caps Lock"
Light get(int which);                 // from zxkb.ini [locks]
void  set(int which, const Light &l); // to zxkb.ini [locks] (the tray rereads it when told settings changed)
void  reload();                       // read zxkb.ini again (the other copy changed it)
bool  any_on();                       // a light is switched on (the tray polls the lock keys only then)

// Tray: called every POLL_MS while any_on(). allowed: the lights may show now (keyboard connected, not idling, no PC
// effect, ZXKB window closed or its lock lights window open). When none should show, the keyboard's own lighting
// (g_st) is put back.
constexpr UINT POLL_MS = 50; // [C] worst delay after the key 50 ms, under the 0.1 s that feels instant [P] (Nielsen, Usability Engineering, 1993)
void tick(bool allowed);
bool showing();               // a lock light is on the keyboard now (its lighting must not be saved)
void put_back();              // put the keyboard's own lighting back now (quitting, the window opening)
void forget();                // the keyboard was disconnected (it lost the light with its power)

// the light's look at time ms: hue, saturation (QMK 0-255) and how bright (0-255 of the brightness used)
void frame(const Light &l, DWORD ms, uint8_t *hue, uint8_t *sat, uint8_t *val);
void dialog(HWND owner); // the Lock lights window (locksdlg.cpp; ZXKB window copy)
int  choose(const bool on[COUNT], const Light lights[COUNT]); // the light to show, -1 for none
} // namespace locklights
