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
extern int       g_pc_effect; // PC effect for standard keyboards: PCFX_NONE or PCFX_PULSE
constexpr int    PCFX_NONE = 0, PCFX_PULSE = 1;
void select_pc_effect(int e);   // standard keyboards: a whole-keyboard effect worked out by ZXKB (PCFX_NONE ends it)
extern bool      g_window_mode; // this copy of ZXKB.exe is the ZXKB window, not the tray
HWND             tray_window(); // the tray copy's hidden window (owns the tray icon), null if not running

constexpr int SIZE_MIN = 1, INTENSITY_MIN = 0; // as ZXKB_SIZE_MIN / ZXKB_INTENSITY_MIN in the firmware (zxkb.h)

// names
std::string  effect_name(int mode);
int          effect_number(const std::string &name);
const wchar_t *vu_label(int style);
std::string  vu_name(int style);
std::wstring menu_name(const std::string &n);
std::wstring current_effect_text();
int          vu_mode();
bool         has_key_layer();    // ZXKB firmware v33 on: "effect off" shows the keys' own colours (the regular layer)
uint8_t      effect_off_mode();  // the effect number sent for "effect off" (0 = lighting off)
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
void enter_bootloader();                      // the keyboard restarts into its bootloader (firmware v17 on)
void settings_changed();                      // tell the other ZXKB copy to read settings (and keyboard files) again
void request_keymap();                        // read the keyboard's keymap (for key labels), if its matrix is known

// Per-key colours and effects, kept on the keyboard (ZXKB firmware v22 on). A key: proto::KEY_* effect, speed 0-255,
// first and second colour. Changes go to the keys given (LED numbers) and are saved with the other settings.
bool per_key_possible();
const uint8_t *key_state(int led);            // 8 bytes: effect, speed 0-255, first r g b, second r g b (or null)
enum KeyEdit { KEY_FIRST, KEY_SECOND, KEY_EFFECT, KEY_SPEED, KEY_FOLLOW };
// KEY_FIRST / KEY_SECOND: colour r, g, b; KEY_EFFECT: value = proto::KEY_*; KEY_SPEED: value 0-255; KEY_FOLLOW: back
// to the keyboard's effect. A key that follows the effect becomes Steady when it is given a colour.
void edit_keys(const std::vector<int> &leds, KeyEdit what, int value, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);

// The Caps Lock light (ZXKB firmware v24 on): shown live, saved with the other settings
bool caps_possible();
CapsLight caps_light();
void set_caps_light(const CapsLight &c);
bool caps_start_keys_possible();               // firmware v25 on
void caps_start_keys();                        // the light back on the keys it started on (the reply updates caps_light)
int  caps_lock_led();
// Ask the keyboard something it answers with a report ZXKB does not otherwise use (e.g. VIA keymap commands) and
// wait briefly for the reply whose first `match` bytes equal the question's. Main thread only.
bool keyboard_ask(const uint8_t *q, int n, uint8_t *reply32, int match);
bool read_keymap_now();                        // read layer 0 of the keymap and wait for it (needs the matrix size)

// Keymap (through VIA). Keys are indices into g_board->keys (the layout); changes are kept by the keyboard at once.
int  layer_count();                            // VIA's dynamic keymap layers (asked once)
int  macro_count();                            // VIA macros the keyboard holds
void show_layer(int layer);                    // read that layer; labels and Change keys follow it
bool key_function(int key, uint16_t *code);    // what the key does on the layer shown
bool set_key_function(int key, uint16_t code); // set it, on the layer shown
// The knob: what turning it does (cw = turning right), on the layer shown; key = the knob in the layout
bool knob_function(int key, bool cw, uint16_t *code);
bool set_knob_function(int key, bool cw, uint16_t code);                          // the LED under the key that is Caps Lock in the keymap, -1 if unknown
