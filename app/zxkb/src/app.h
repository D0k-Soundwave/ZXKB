// ZXKB app state and actions, shared by the tray menu (main.cpp) and the ZXKB window (window.cpp).
// Both drive the keyboard through these, so they always agree.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "hidlink.h"

// Spectrum BRIGHT palette as QMK hue/saturation (with D0k's calibrated colour balance the pure hues match)
struct Colour {
    const wchar_t *name;
    uint8_t        hue, sat;
    COLORREF       swatch; // how it looks on the keyboard after calibration (CLAUDE.md), for drawing
};
extern const Colour colours[7];

extern HINSTANCE g_inst;
extern HWND      g_hwnd;     // the app's hidden window (tray messages, keyboard messages)
extern Board    *g_board;    // null while no keyboard is connected
extern Status    g_st;       // last known keyboard state
extern Tune      g_tune;     // size and intensity of the current effect
extern int       g_vu_style; // VuStyle
extern bool      g_window_mode; // this copy of ZXKB.exe is the ZXKB window, not the tray
HWND             tray_window(); // the tray copy's hidden window (owns the tray icon), null if not running

constexpr int TUNE_MIN = 16; // must match ZXKB_TUNE_MIN in the firmware (zxkb.h)

// names
std::string  effect_name(int mode);
int          effect_number(const std::string &name);
const wchar_t *vu_label(int style);
std::string  vu_name(int style);
std::wstring menu_name(const std::string &n);
std::wstring current_effect_text();
int          vu_mode();
bool         vu_showing();
bool         replaced(size_t i);                 // QMK effect hidden because the board has ZXKB's version
std::string  current_name();                     // effect or "VU:style" now showing, "" if none

// actions
void set_effect(int mode); // 0 = lighting off
void select_vu(int style);
void set_brightness(int val);
void set_speed(int speed);
void set_tune(int size, int intensity);
void set_colour(uint8_t hue, uint8_t sat);
void refresh_status();     // ask the keyboard for its state and wait briefly for the reply
bool is_favourite(const std::string &n);
void keyboard_mirror(uint8_t *rgb, int leds); // the LED colours the keyboard last reported
void mirror_request(bool on);                 // ask the keyboard to keep sending its LED colours (for 2 s)
void set_favourite(const std::string &n, bool on);
void enter_bootloader();
void settings_changed();
void request_keymap();
bool per_key_possible();                      // the keyboard keeps per-key colours (ZXKB firmware v19 on)
// Give keys (LED numbers) their own colour (on) or hand them back to the effect (off); kept on the keyboard
void set_key_colours(const std::vector<int> &leds, bool on, uint8_t r, uint8_t g, uint8_t b);                        // read the keyboard's keymap (for key labels), if its matrix is known                      // tell the other ZXKB copy to read settings (and keyboard files) again                      // the keyboard restarts into its bootloader (firmware v17 on)
